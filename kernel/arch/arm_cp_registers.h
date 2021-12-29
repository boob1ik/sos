/*!
 * @file  arm_cp_registers.h
 * @brief Definitions for ARM coprocessor registers.
 */

#ifndef __ARM_CP_REGISTERS_H__
#define __ARM_CP_REGISTERS_H__

//! @name ACTLR
//@{
#define BM_ACTLR_SMP (1 << 6)
#define BM_ACTLR_ZERO (1 << 3) //!< Write full line of zeros mode
#define BM_ACTLR_L1  (1 << 2)  //!< L1 Prefetch enable
#define BM_ACTLR_L2  (1 << 1)  //!< L2 Prefetch hint enable
//@}

//! @name DFSR
//@{
#define BM_DFSR_WNR (1 << 11)   //!< Write not Read bit. 0=read, 1=write.
#define BM_DFSR_FS4 (0x400)      //!< Fault status bit 4..
#define BP_DFSR_FS4 (10)        //!< Bit position for FS[4].
#define BM_DFSR_FS (0xf)      //!< Fault status bits [3:0].
//@}

//! @name SCTLR
//@{
#define BM_SCTLR_TE (1 << 30)  //!< Thumb exception enable.
#define BM_SCTLR_AFE (1 << 29) //!< Access flag enable.
#define BM_SCTLR_TRE (1 << 28) //!< TEX remap enable.
#define BM_SCTLR_NMFI (1 << 27)    //!< Non-maskable FIQ support.
#define BM_SCTLR_EE (1 << 25)  //!< Exception endianess.
#define BM_SCTLR_VE (1 << 24)  //!< Interrupt vectors enable.
#define BM_SCTLR_FI (1 << 21)   //!< Fast interrupt configurable enable.
#define BM_SCTLR_RR (1 << 14)  //!< Round Robin
#define BM_SCTLR_V (1 << 13)   //!< Vectors
#define BM_SCTLR_I (1 << 12)   //!< Instruction cache enable
#define BM_SCTLR_Z (1 << 11)   //!< Branch prediction enable
#define BM_SCTLR_SW (1 << 10)  //!< SWP and SWPB enable
#define BM_SCTLR_CP15BEN (1 << 5)  //!< CP15 barrier enable
#define BM_SCTLR_C (1 << 2)    //!< Data cache enable
#define BM_SCTLR_A (1 << 1)    //!< Alignment check enable
#define BM_SCTLR_M (1 << 0)    //!< MMU enable
//@}

//! @}

#endif
