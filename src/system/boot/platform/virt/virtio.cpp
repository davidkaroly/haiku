/*
 * Copyright 2021-2022, Haiku, Inc.
 * Distributed under the terms of the MIT License.
 */


#include "virtio.h"

#include <new>
#include <string.h>
#include <kernel.h>
#include <malloc.h>
#include <virtio.h>
#include <KernelExport.h>


//#define TRACE_VIRTIO
#ifdef TRACE_VIRTIO
#	define TRACE(x...) dprintf("virtio: " x)
#else
#	define TRACE(x...) ;
#endif

enum {
	maxVirtioDevices = 32,
};


VirtioResources gVirtioDevList[maxVirtioDevices];
int32_t gVirtioDevListLen = 0;

DoublyLinkedList<VirtioDevice> gVirtioDevices;
VirtioDevice* gKeyboardDev = NULL;


void*
aligned_malloc(size_t required_bytes, size_t alignment)
{
    void* p1; // original block
    void** p2; // aligned block
    int offset = alignment - 1 + sizeof(void*);
    if ((p1 = (void*)malloc(required_bytes + offset)) == NULL) {
       return NULL;
    }
    p2 = (void**)(((size_t)(p1) + offset) & ~(alignment - 1));
    p2[-1] = p1;
    return p2;
}


void
aligned_free(void* p)
{
    free(((void**)p)[-1]);
}


static const char *
virtio_get_feature_name(uint32_t feature)
{
	switch (feature) {
		case VIRTIO_FEATURE_NOTIFY_ON_EMPTY:
			return "notify on empty";
		case VIRTIO_FEATURE_ANY_LAYOUT:
			return "any layout";
		case VIRTIO_FEATURE_RING_INDIRECT_DESC:
			return "ring indirect";
		case VIRTIO_FEATURE_RING_EVENT_IDX:
			return "ring event index";
		case VIRTIO_FEATURE_BAD_FEATURE:
			return "bad feature";
	}
	return NULL;
}


static void
virtio_dump_features(const char* title, uint32_t features)
{
	char features_string[512] = "";
	for (uint32_t i = 0; i < 32; i++) {
		uint32 feature = features & (1 << i);
		if (feature == 0)
			continue;
		const char* name = virtio_get_feature_name(feature);
		//if (name == NULL)
		//	name = get_feature_name(feature);
		if (name != NULL) {
			strlcat(features_string, "[", sizeof(features_string));
			strlcat(features_string, name, sizeof(features_string));
			strlcat(features_string, "] ", sizeof(features_string));
		}
	}
	TRACE("%s: 0x%08x %s\n", title, features, features_string);
}


VirtioResources*
find_virtio_dev(uint32 deviceId, int n)
{
	for (int i = 0; i < gVirtioDevListLen; i++) {
		VirtioRegs* volatile regs = gVirtioDevList[i].regs;
		if (regs->signature != kVirtioSignature)
			continue;
		if (regs->deviceId == deviceId) {
			if (n == 0)
				return &gVirtioDevList[i];
			else
				n--;
		}
	}
	return NULL;
}


//#pragma mark VirtioDevice

int32_t
VirtioDevice::AllocDesc()
{
	for (size_t i = 0; i < fQueueLen; i++) {
		if ((fFreeDescs[i/32] & (1 << (i % 32))) != 0) {
			fFreeDescs[i/32] &= ~((uint32_t)1 << (i % 32));
			return i;
		}
	}
	return -1;
}


void
VirtioDevice::FreeDesc(int32_t idx)
{
	fFreeDescs[idx/32] |= (uint32_t)1 << (idx % 32);
}


VirtioDevice::VirtioDevice(const VirtioResources& devRes): fRegs(devRes.regs)
{
	gVirtioDevices.Insert(this);

	TRACE("+VirtioDevice @ 0x%08x\n", (uint32_t)fRegs);

	fRegs->status = 0; // reset

	fRegs->status |= kVirtioConfigSAcknowledge;
	fRegs->status |= kVirtioConfigSDriver;

	uint32_t features = fRegs->deviceFeatures;
	virtio_dump_features("read features", features);

	features &= (VIRTIO_FEATURE_TRANSPORT_MASK
		| VIRTIO_FEATURE_RING_INDIRECT_DESC | VIRTIO_FEATURE_RING_EVENT_IDX);

	fRegs->driverFeatures = features;
	virtio_dump_features("negotiated features", features);

	fRegs->status |= kVirtioConfigSFeaturesOk;
	fRegs->status |= kVirtioConfigSDriverOk;

	if (fRegs->version == 1) {
		fRegs->guestPageSize = B_PAGE_SIZE;
	}

	fRegs->queueSel = 0;
	TRACE("queueNumMax: %d\n", fRegs->queueNumMax);
	fQueueLen = fRegs->queueNumMax;
	fRegs->queueNum = fQueueLen;
	fLastUsed = 0;

	uint32_t fDescsSize = ROUNDUP(sizeof(VirtioDesc) * fQueueLen, B_PAGE_SIZE);
	uint32_t fAvailSize = ROUNDUP(sizeof(VirtioAvail) + sizeof(uint16_t) * fQueueLen, B_PAGE_SIZE);
	uint32_t fUsedSize = ROUNDUP(sizeof(VirtioUsedItem) * fQueueLen, B_PAGE_SIZE);

	uint32_t bufferSize = fDescsSize + fAvailSize + fUsedSize;
	void *buffer = aligned_malloc(bufferSize, B_PAGE_SIZE);

	fDescs = (VirtioDesc *)buffer;
	fAvail = (VirtioAvail *)((addr_t)buffer + fDescsSize);
	fUsed = (VirtioUsed *)((addr_t)buffer + fDescsSize + fAvailSize);

	memset(fDescs, 0, fDescsSize);
	memset(fAvail, 0, fAvailSize);
	memset(fUsed, 0, fUsedSize);

	fFreeDescs = new(std::nothrow) uint32_t[(fQueueLen + 31)/32];
	memset(fFreeDescs, 0xff, sizeof(uint32_t) * ((fQueueLen + 31)/32));

	fReqs = new(std::nothrow) IORequest*[fQueueLen];

	if (fRegs->version != 1) {
		fRegs->queueDescLow = (uint32_t)(uint64_t)fDescs;
		fRegs->queueDescHi = (uint32_t)((uint64_t)fDescs >> 32);
		fRegs->queueAvailLow = (uint32_t)(uint64_t)fAvail;
		fRegs->queueAvailHi = (uint32_t)((uint64_t)fAvail >> 32);
		fRegs->queueUsedLow = (uint32_t)(uint64_t)fUsed;
		fRegs->queueUsedHi = (uint32_t)((uint64_t)fUsed >> 32);
	}

	TRACE("fDescs: %p\n", fDescs);
	TRACE("fAvail: %p\n", fAvail);
	TRACE("fUsed: %p\n", fUsed);

	if (fRegs->version == 1) {
		uint32_t pfn = (uint32_t)(addr_t)fDescs / B_PAGE_SIZE;
		fRegs->queueAlign = B_PAGE_SIZE;
		fRegs->queuePfn = pfn;
	} else {
		fRegs->queueReady = 1;
	}

	fRegs->config[0] = kVirtioInputCfgIdName;
//	dprintf("name: %s\n", (const char*)(&fRegs->config[8]));
}


VirtioDevice::~VirtioDevice()
{
	gVirtioDevices.Remove(this);
	fRegs->status = 0; // reset
}


void
VirtioDevice::ScheduleIO(IORequest** reqs, uint32 cnt)
{
	if (cnt < 1) return;
	int32_t firstDesc, lastDesc;
	for (uint32 i = 0; i < cnt; i++) {
		int32_t desc = AllocDesc();
		if (desc < 0) {panic("virtio: no more descs"); return;}
		if (i == 0) {
			firstDesc = desc;
		} else {
			fDescs[lastDesc].flags |= kVringDescFlagsNext;
			fDescs[lastDesc].next = desc;
			reqs[i - 1]->next = reqs[i];
		}
		fDescs[desc].addr = (uint64_t)(reqs[i]->buf);
		fDescs[desc].len = reqs[i]->len;
		fDescs[desc].flags = 0;
		fDescs[desc].next = 0;
		switch (reqs[i]->op) {
		case ioOpRead: break;
		case ioOpWrite: fDescs[desc].flags |= kVringDescFlagsWrite; break;
		}
		reqs[i]->state = ioStatePending;
		lastDesc = desc;
	}
	int32_t idx = fAvail->idx % fQueueLen;
	fReqs[idx] = reqs[0];
	fAvail->ring[idx] = firstDesc;
	fAvail->idx++;
	fRegs->queueNotify = 0;
}


void
VirtioDevice::ScheduleIO(IORequest* req)
{
	ScheduleIO(&req, 1);
}


IORequest*
VirtioDevice::ConsumeIO()
{
	if (fUsed->idx == fLastUsed)
		return NULL;

	IORequest* req = fReqs[fLastUsed % fQueueLen];
	fReqs[fLastUsed % fQueueLen] = NULL;
	req->state = ioStateDone;
	int32 desc = fUsed->ring[fLastUsed % fQueueLen].id;
	while (kVringDescFlagsNext & fDescs[desc].flags) {
		int32 nextDesc = fDescs[desc].next;
		FreeDesc(desc);
		desc = nextDesc;
	}
	FreeDesc(desc);
	fLastUsed++;
	return req;
}


IORequest*
VirtioDevice::WaitIO()
{
	while (fUsed->idx == fLastUsed) {}
	return ConsumeIO();
}


//#pragma mark -

void
virtio_register(addr_t base, size_t len, uint32 irq)
{
	VirtioRegs* volatile regs = (VirtioRegs* volatile)base;

	TRACE("virtio_register(0x%" B_PRIxADDR ", 0x%" B_PRIxSIZE ", "
		"%" B_PRIu32 ")\n", base, len, irq);
	TRACE("  signature: 0x%" B_PRIx32 "\n", regs->signature);
	TRACE("  version: %" B_PRIu32 "\n", regs->version);
	TRACE("  device id: %" B_PRIu32 "\n", regs->deviceId);

	if (!(gVirtioDevListLen < maxVirtioDevices)) {
		TRACE("too many VirtIO devices\n");
		return;
	}
	gVirtioDevList[gVirtioDevListLen].regs = regs;
	gVirtioDevList[gVirtioDevListLen].regsSize = len;
	gVirtioDevList[gVirtioDevListLen].irq = irq;
	gVirtioDevListLen++;
}


void
virtio_init()
{
	TRACE("virtio_init()\n");

	int i = 0;
	for (; ; i++) {
		VirtioResources* devRes = find_virtio_dev(kVirtioDevInput, i);
		if (devRes == NULL) break;
		VirtioRegs* volatile regs = devRes->regs;
		regs->config[0] = kVirtioInputCfgIdName;
		TRACE("virtio_input[%d]: %s\n", i, (const char*)(&regs->config[8]));
		if (i == 0)
			gKeyboardDev = new(std::nothrow) VirtioDevice(*devRes);
	}
	TRACE("virtio_input count: %d\n", i);
	if (gKeyboardDev != NULL) {
		for (int i = 0; i < 4; i++) {
			gKeyboardDev->ScheduleIO(new(std::nothrow) IORequest(ioOpWrite,
				malloc(sizeof(VirtioInputPacket)), sizeof(VirtioInputPacket)));
		}
	}
}


void
virtio_fini()
{
	auto it = gVirtioDevices.GetIterator();
	while (VirtioDevice* dev = it.Next()) {
		dev->Regs()->status = 0; // reset
	}
}


int
virtio_input_get_key()
{
	if (gKeyboardDev == NULL)
		return 0;

	IORequest* req = gKeyboardDev->ConsumeIO();
	if (req == NULL)
		return 0;

	VirtioInputPacket &pkt = *(VirtioInputPacket*)req->buf;

	int key = 0;
	if (pkt.type == 1 && pkt.value == 1)
		key = pkt.code;

	free(req->buf);
	req->buf = NULL;
	delete req;

	gKeyboardDev->ScheduleIO(new(std::nothrow) IORequest(ioOpWrite,
		malloc(sizeof(VirtioInputPacket)), sizeof(VirtioInputPacket)));

	return key;
}


int
virtio_input_wait_for_key()
{
	int key = 0;

	do {
		key = virtio_input_get_key();
	} while (key == 0);

	return key;
}
