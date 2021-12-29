#ifndef GIC_H_
#define GIC_H_

#define GIC_PRIORITY_MIN    (0xffu)
#define GIC_PRIORITY_MAX    (0u)

typedef struct {
    vuint32_t ICDDCR;           /**< Distributor Control Register */
    vuint32_t ICDICTR;          /**< Interrupt Controller Type Register */
    vuint32_t ICDIIDR;          /**< Distributor Implementer Identification Register */
    uint8_t         resrv1[116];
    vuint32_t ICDISR[32];       /**< Interrupt Security Registers */
    vuint32_t ICDISER[32];      /**< Interrupt Set-Enable Registers */
    vuint32_t ICDICER[32];      /**< Interrupt Clear-Enable Registers */
    vuint32_t ICDISPR[32];      /**< Interrupt Set-Pending Registers */
    vuint32_t ICDICPR[32];      /**< Interrupt Clear-Pending Registers */
    vuint32_t ICDABR[32];       /**< Active Bit Registers */
    uint8_t         resrv2[128];
    vuint32_t ICDIPR[256];      /**< Interrupt Priority Registers */
    vuint32_t ICDIPTR[256];     /**< Interrupt Processor Targets Registers */
    vuint32_t ICDICFR[64];      /**< Interrupt Configuration Registers */
    vuint32_t ICDPPIS[1];       /**< Private Peripheral Interrupt Status Register */
    vuint32_t ICDSPIS[7];       /**< Shared Peripheral Interrupt Status Registers */
    uint8_t         resrv3[480];
    vuint32_t ICDSGIR;          /**< Software Generated Interrupt Register */
    uint8_t         resrv12[204];
    uint8_t PeriphID[8];       /**< Peripheral Identification Registers */
    uint8_t CompID[4];         /**< Component Identification Registers */

} GIC_Distributor_t;

typedef struct {
    vuint32_t ICCICR;           /**< Processor Interface Control Register */
    vuint32_t ICCPMR;           /**< Priority Mask Register */
    vuint32_t ICCBPR;           /**< Binary Point Register */
    vuint32_t ICCIAR;           /**< Interrupt Acknowledge Register */
    vuint32_t ICCEOIR;          /**< End Of Interrupt Register */
    vuint32_t ICCRPR;           /**< Running Priority Register */
    vuint32_t ICCHPIR;          /**< Highest Pending Interrupt Register */
    vuint32_t ICCABPR;          /**< Aliased Non-secure Binary Point Register */
    uint8_t         resrv12[220];
    vuint32_t ICCIIDR;          /**< Processor Interface Implementer Identification Register*/

} GIC_CPU_IFace_t;

#define GIC_BASE         (0xa00000u)

#define GIC_ICC_BASE     (GIC_BASE + 0x100u)
#define GIC_ICD_BASE     (GIC_BASE + 0x1000u)

#define GIC_ICC     ((GIC_CPU_IFace_t *)GIC_ICC_BASE)
#define GIC_ICD     ((GIC_Distributor_t *)GIC_ICD_BASE)


#endif /* GIC_H_ */
