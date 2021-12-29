#include "pl310_l2cc.h"
#include <stddef.h>

struct pl310_l2cc {
    // Register r0
    const volatile unsigned int CacheID;              // 0x000
    const volatile unsigned int CacheType;            // 0x004
    volatile unsigned int Reserved0[(0x100 - 0x08) / 4];  // 0x008-0x0FC

    // Register r1
    volatile unsigned int Ctrl;                       // 0x100
    volatile unsigned int AuxCtrl;                    // 0x104
    volatile unsigned int TagRAMLatencyCtrl;          // 0x108
    volatile unsigned int DataRAMLatencyCtrl;         // 0x10C
    volatile unsigned int Reserved1[(0x200 - 0x110) / 4]; // 0x110-0x1FC

    // Register r2
    volatile unsigned int EvtCtrCtrl;                 // 0x200
    volatile unsigned int EvtCtr1Cnfg;                // 0x204
    volatile unsigned int EvtCtr0Cnfg;                // 0x208
    volatile unsigned int EvtCtr1Val;                 // 0x20C
    volatile unsigned int EvtCtr0Val;                 // 0x210
    volatile unsigned int IntrMask;                   // 0x214
    const volatile unsigned int MaskIntrStatus;       // 0x218
    const volatile unsigned int RawIntrStatus;        // 0x21C
    volatile unsigned int IntrClr;                    // 0x220 (WO)
    volatile unsigned int Reserved2[(0x730 - 0x224) / 4]; // 0x224-0x72c

    // Register r7
    volatile unsigned int CacheSync;                   // 0x730
    volatile unsigned int Reserved71[(0x770 - 0x734) / 4]; // 0x734-0x76C
    volatile unsigned int InvalLineByPA;               // 0x770
    volatile unsigned int Reserved72[(0x77C - 0x774) / 4]; // 0x774-0x778
    volatile unsigned int InvalByWay;                  // 0x77C
    volatile unsigned int Reserved73[(0x7B0 - 0x780) / 4]; // 0x780-0x7AC
    volatile unsigned int CleanLineByPA;               // 0x7B0
    volatile unsigned int Reserved74;                  // 0x7B4
    volatile unsigned int CleanLineByIndexWay;         // 0x7B8
    volatile unsigned int CleanByWay;                  // 0x7BC
    volatile unsigned int Reserved75[(0x7F0 - 0x7C0) / 4]; // 0x7C0-0x7EC
    volatile unsigned int CleanInvalByPA;              // 0x7F0
    volatile unsigned int Reserved76;                  // 0x7F4
    volatile unsigned int CleanInvalByIndexWay;        // 0x7F8
    volatile unsigned int CleanInvalByWay;             // 0x7FC
    volatile unsigned int Reserved77[(0x900 - 0x800) / 4]; // 0x800-0x8FC

    // Register r9
    volatile unsigned int DataLockdown0ByWay;          // 0x900
    volatile unsigned int InstrLockdown0ByWay;         // 0x904
    volatile unsigned int DataLockdown1ByWay;          // 0x908
    volatile unsigned int InstrLockdown1ByWay;         // 0x90C
    volatile unsigned int DataLockdown2ByWay;          // 0x910
    volatile unsigned int InstrLockdown2ByWay;         // 0x914
    volatile unsigned int DataLockdown3ByWay;          // 0x918
    volatile unsigned int InstrLockdown3ByWay;         // 0x91C
    volatile unsigned int DataLockdown4ByWay;          // 0x920
    volatile unsigned int InstrLockdown4ByWay;         // 0x924
    volatile unsigned int DataLockdown5ByWay;          // 0x928
    volatile unsigned int InstrLockdown5ByWay;         // 0x92C
    volatile unsigned int DataLockdown6ByWay;          // 0x930
    volatile unsigned int InstrLockdown6ByWay;         // 0x934
    volatile unsigned int DataLockdown7ByWay;          // 0x938
    volatile unsigned int InstrLockdown7ByWay;         // 0x93C
    volatile unsigned int Reserved90[(0x950 - 0x940) / 4]; // 0x940-0x94C
    volatile unsigned int LockdownByLineEnable;        // 0x950
    volatile unsigned int UnlockAllLinesByWay;         // 0x954
    volatile unsigned int Reserved91[(0xC00 - 0x958) / 4]; // 0x958-0x9FC

    // Register r12
    volatile unsigned int AddressFilteringStart;       // 0xC00
    volatile unsigned int AddressFilteringEnd;         // 0xC04
    volatile unsigned int Reserved12[(0xF40 - 0xC08) / 4]; // 0xC08-0xF3C

    // Register r15
    volatile unsigned int DebugCtrl;                   // 0xF40
};

static struct pl310_l2cc *l2 = (struct pl310_l2cc *)0x00A02000;

// Accessor functions

// Register r0
unsigned get_pl310_l2cc_CacheID ()
{
    return l2->CacheID;
}

unsigned get_pl310_l2cc_CacheType ()
{
    return l2->CacheType;
}

// Register r1
void pl310_l2cc_enable (unsigned int set)
{
    l2->Ctrl = set;
}

unsigned get_pl310_l2cc_AuxCtrl (void)
{
    return l2->AuxCtrl;
}

void set_pl310_l2cc_AuxCtrl (unsigned data)
{
    l2->AuxCtrl = data;
}

unsigned get_pl310_l2cc_TagRAMLatencyCtrl (void)
{
    return l2->TagRAMLatencyCtrl;
}

void set_pl310_l2cc_TagRAMLatencyCtrl (unsigned data)
{
    l2->TagRAMLatencyCtrl = data;
}

unsigned get_pl310_l2cc_DataRAMLatencyCtrl (void)
{
    return l2->DataRAMLatencyCtrl;
}

void set_pl310_l2cc_DataRAMLatencyCtrl (unsigned data)
{
    l2->DataRAMLatencyCtrl = data;
}

// Register r2
unsigned get_pl310_l2cc_EvtCtrCtrl (void)
{
    return l2->EvtCtrCtrl;
}

void set_pl310_l2cc_EvtCtrCtrl (unsigned data)
{
    l2->EvtCtrCtrl = data;
}

unsigned get_pl310_l2cc_EvtCtr1Cnfg (void)
{
    return l2->EvtCtr1Cnfg;
}

void set_pl310_l2cc_EvtCtr1Cnfg (unsigned data)
{
    l2->EvtCtr1Cnfg = data;
}

unsigned get_pl310_l2cc_EvtCtr0Cnfg (void)
{
    return l2->EvtCtr0Cnfg;
}

void set_pl310_l2cc_EvtCtr0Cnfg (unsigned data)
{
    l2->EvtCtr0Cnfg = data;
}

unsigned get_pl310_l2cc_EvtCtr1Val (void)
{
    return l2->EvtCtr1Val;
}

void set_pl310_l2cc_EvtCtr1Val (unsigned data)
{
    l2->EvtCtr1Val = data;
}

unsigned get_pl310_l2cc_EvtCtr0Val (void)
{
    return l2->EvtCtr0Val;
}

void set_pl310_l2cc_EvtCtr0Val (unsigned data)
{
    l2->EvtCtr0Val = data;
}

unsigned get_pl310_l2cc_IntrMask (void)
{
    return l2->IntrMask;
}

void set_pl310_l2cc_IntrMask (unsigned data)
{
    l2->IntrMask = data;
}

unsigned get_pl310_l2cc_MaskIntrStatus (void)
{
    return l2->MaskIntrStatus;
}

unsigned get_pl310_l2cc_RawIntrStatus (void)
{
    return l2->RawIntrStatus;
}

void set_pl310_l2cc_IntrClr (unsigned data)
{
    l2->IntrClr = data;
}

// Register r7
unsigned get_pl310_l2cc_CacheSync (void)
{
    return l2->CacheSync;
}

void set_pl310_l2cc_CacheSync (unsigned data)
{
    l2->CacheSync = data;
}

unsigned get_pl310_l2cc_InvalLineByPA (void)
{
    return l2->InvalLineByPA;
}

void set_pl310_l2cc_InvalLineByPA (unsigned data)
{
    l2->InvalLineByPA = data;
}

unsigned get_pl310_l2cc_InvalByWay (void)
{
    return l2->InvalByWay;
}

void set_pl310_l2cc_InvalByWay (unsigned data)
{
    l2->InvalByWay = data;
}

unsigned get_pl310_l2cc_CleanLineByPA (void)
{
    return l2->CleanLineByPA;
}

void set_pl310_l2cc_CleanLineByPA (unsigned data)
{
    l2->CleanLineByPA = data;
}

unsigned get_pl310_l2cc_CleanLineByIndexWay (void)
{
    return l2->CleanLineByIndexWay;
}

void set_pl310_l2cc_CleanLineByIndexWay (unsigned data)
{
    l2->CleanLineByIndexWay = data;
}

unsigned get_pl310_l2cc_CleanByWay (void)
{
    return l2->CleanByWay;
}

void set_pl310_l2cc_CleanByWay (unsigned data)
{
    l2->CleanByWay = data;
}

unsigned get_pl310_l2cc_CleanInvalByPA (void)
{
    return l2->CleanInvalByPA;
}

void set_pl310_l2cc_CleanInvalByPA (unsigned data)
{
    l2->CleanInvalByPA = data;
}

unsigned get_pl310_l2cc_CleanInvalByIndexWay (void)
{
    return l2->CleanInvalByIndexWay;
}

void set_pl310_l2cc_CleanInvalByIndexWay (unsigned data)
{
    l2->CleanInvalByIndexWay = data;
}

unsigned get_pl310_l2cc_CleanInvalByWay (void)
{
    return l2->CleanInvalByWay;
}

void set_pl310_l2cc_CleanInvalByWay (unsigned data)
{
    l2->CleanInvalByWay = data;
}

// Register r9
unsigned get_pl310_l2cc_DataLockdownByWay (unsigned master_id)
{
    switch (master_id) {
    case 0:
        return l2->DataLockdown0ByWay;
    case 1:
        return l2->DataLockdown1ByWay;
    case 2:
        return l2->DataLockdown2ByWay;
    case 3:
        return l2->DataLockdown3ByWay;
    case 4:
        return l2->DataLockdown4ByWay;
    case 5:
        return l2->DataLockdown5ByWay;
    case 6:
        return l2->DataLockdown6ByWay;
    case 7:
        return l2->DataLockdown7ByWay;
    default:
        return 0;
    }
}

void set_pl310_l2cc_DataLockdownByWay (unsigned master_id, unsigned data)
{
    switch (master_id) {
    case 0:
        l2->DataLockdown0ByWay = data;
    case 1:
        l2->DataLockdown1ByWay = data;
    case 2:
        l2->DataLockdown2ByWay = data;
    case 3:
        l2->DataLockdown3ByWay = data;
    case 4:
        l2->DataLockdown4ByWay = data;
    case 5:
        l2->DataLockdown5ByWay = data;
    case 6:
        l2->DataLockdown6ByWay = data;
    case 7:
        l2->DataLockdown7ByWay = data;
    }
}

unsigned get_pl310_l2cc_InstrLockdownByWay (unsigned master_id)
{
    switch (master_id) {
    case 0:
        return l2->InstrLockdown0ByWay;
    case 1:
        return l2->InstrLockdown1ByWay;
    case 2:
        return l2->InstrLockdown2ByWay;
    case 3:
        return l2->InstrLockdown3ByWay;
    case 4:
        return l2->InstrLockdown4ByWay;
    case 5:
        return l2->InstrLockdown5ByWay;
    case 6:
        return l2->InstrLockdown6ByWay;
    case 7:
        return l2->InstrLockdown7ByWay;
    default:
        return 0;
    }
}

void set_pl310_l2cc_InstrLockdownByWay (unsigned master_id, unsigned data)
{
    switch (master_id) {
    case 0:
        l2->InstrLockdown0ByWay = data;
    case 1:
        l2->InstrLockdown1ByWay = data;
    case 2:
        l2->InstrLockdown2ByWay = data;
    case 3:
        l2->InstrLockdown3ByWay = data;
    case 4:
        l2->InstrLockdown4ByWay = data;
    case 5:
        l2->InstrLockdown5ByWay = data;
    case 6:
        l2->InstrLockdown6ByWay = data;
    case 7:
        l2->InstrLockdown7ByWay = data;
    }
}

unsigned get_pl310_l2cc_LockdownByLineEnable (void)
{
    return l2->LockdownByLineEnable;
}

void set_pl310_l2cc_LockdownByLineEnable (unsigned data)
{
    l2->LockdownByLineEnable = data;
}

unsigned get_pl310_l2cc_UnlockAllLinesByWay (void)
{
    return l2->UnlockAllLinesByWay;
}

void set_pl310_l2cc_UnlockAllLinesByWay (unsigned data)
{
    l2->UnlockAllLinesByWay = data;
}

// Register r12
unsigned get_pl310_l2cc_AddrFilteringStart (void)
{
    return l2->AddressFilteringStart;
}

void set_pl310_l2cc_AddrFilteringStart (unsigned data)
{
    l2->AddressFilteringStart = data;
}

unsigned get_pl310_l2cc_AddrFilteringEnd (void)
{
    return l2->AddressFilteringEnd;
}

void set_pl310_l2cc_AddrFilteringEnd (unsigned data)
{
    l2->AddressFilteringEnd = data;
}

// Register r15
unsigned get_pl310_l2cc_DebugCtrl (void)
{
    return l2->DebugCtrl;
}

void set_pl310_l2cc_DebugCtrl (unsigned data)
{
    l2->DebugCtrl = data;
}

void init_pl310_l2cc ()
{
    pl310_l2cc_enable(0x00000000);

    // PBX-A9 settings: 8-way, 16kb way-size, event monitor bus enable,
    // shared override, tag/data RAM latency = 0
    //set_pl310_l2cc_AuxCtrl(0x00520000);
    //set_pl310_l2cc_TagRAMLatencyCtrl(0);
    //set_pl310_l2cc_DataRAMLatencyCtrl(0);

    //set_pl310_l2cc_IntrClr(0x000001ff);

    //set_pl310_l2cc_IntrMask(0x000001ff);

    set_pl310_l2cc_InvalByWay(0x0000ffff);
    while (get_pl310_l2cc_InvalByWay());
}
