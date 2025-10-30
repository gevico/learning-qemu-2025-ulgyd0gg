#ifndef HW_G233_SPI_H 
#define HW_G233_SPI_H 

#include "hw/sysbus.h"

#define TYPE_G233_SPI "g233-spi"

OBJECT_DECLARE_SIMPLE_TYPE(G233SPIState, G233_SPI)

#define G233_SPI(obj) \
    OBJECT_CHECK(G233SPIState, (obj), TYPE_G233_SPI)

/* SPI Status Register (SR) bits */
#define SPI_SR_OVERRUN  (1 << 3)
#define SPI_SR_UNDERRUN (1 << 2)
#define SPI_SR_TXE      (1 << 1)   /* Transmit buffer empty */
#define SPI_SR_RXNE     (1 << 0)   /* Receive buffer not empty */
#define SPI_SR_BSY      (1 << 7)   /* Busy flag */
#define SPI_CR2_TXEIE   (1 << 7)
#define SPI_CR2_RXNEIE  (1 << 6)
#define SPI_CR2_ERRIE   (1 << 5)
#define SPI_CR2_SSOE    (1 << 4)

struct G233SPIState
{
    SysBusDevice parent_obj;
    
    MemoryRegion mmio;
    SSIBus *ssi;
    qemu_irq irq;
    uint32_t spi_cr1;
    uint32_t spi_cr2;
    uint32_t spi_sr;
    uint32_t spi_dr;
    uint32_t spi_csctrl;
    qemu_irq cs_line[4];
};

enum {
    SPI_CR1 =    0x00,
    SPI_CR2 =    0x04,
    SPI_SR  =    0x08,
    SPI_DR  =    0x0C,
    SPI_CSCTRL = 0x10
};

#endif