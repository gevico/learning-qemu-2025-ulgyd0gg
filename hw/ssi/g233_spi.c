#include "qemu/osdep.h"
#include "qemu/module.h"
#include "migration/vmstate.h"
#include "hw/sysbus.h"
#include "hw/ssi/ssi.h"
#include "hw/ssi/g233_spi.h"
#include "hw/irq.h"

static const VMStateDescription vmstate_g233_spi = {
    .name = TYPE_G233_SPI,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32(spi_cr1, G233SPIState),
        VMSTATE_UINT32(spi_cr2, G233SPIState),
        VMSTATE_UINT32(spi_sr, G233SPIState),
        VMSTATE_UINT32(spi_dr, G233SPIState),
        VMSTATE_UINT32(spi_csctrl, G233SPIState),
        VMSTATE_END_OF_LIST()
    }
};

static void g233_spi_update_cs(G233SPIState *s) 
{
    for (int i = 0; i < 4; i++) {
        bool enabled = s->spi_csctrl & (1 << i);
        bool active = s->spi_csctrl & (1 << (i + 4));

        qemu_set_irq(s->cs_line[i], enabled && active ? 0 : 1);
    }
}

static void g233_spi_update_irq(G233SPIState *s)
{
    bool irq = false;

    if ((s->spi_cr2 & SPI_CR2_TXEIE) && (s->spi_sr & SPI_SR_TXE)) {
        irq = true;
    }
    if ((s->spi_cr2 & SPI_CR2_RXNEIE) && (s->spi_sr & SPI_SR_RXNE)) {
        irq = true;
    }
    if (s->spi_cr2 & SPI_CR2_ERRIE) {
        if ((s->spi_sr & SPI_SR_UNDERRUN) || 
            (s->spi_sr & SPI_SR_OVERRUN)) {
            irq = true;
        }
    }
    qemu_set_irq(s->irq, irq);
}

static void g233_spi_transfer(G233SPIState *s)
{
    s->spi_sr |= SPI_SR_BSY;
    uint32_t value = ssi_transfer(s->ssi, s->spi_dr);

    if (s->spi_sr & SPI_SR_RXNE) {
        s->spi_sr |= SPI_SR_OVERRUN;
        g233_spi_update_irq(s);
    } else {
        s->spi_dr = value;
    }
    s->spi_sr |= SPI_SR_TXE;
    s->spi_sr |= SPI_SR_RXNE;
    s->spi_sr &= ~SPI_SR_BSY;
    g233_spi_update_irq(s);
}

static void g233_spi_write(void *opaque, hwaddr offset,
                           uint64_t val, unsigned size)
{
    G233SPIState *s = opaque;
    uint32_t value = val;
    switch(offset) {
        case SPI_CR1:
            s->spi_cr1 = value;
            return ;
        case SPI_CR2:
            s->spi_cr2 = value;
            g233_spi_update_irq(s);
            return ;
        case SPI_SR:
            if (value & SPI_SR_OVERRUN) {
                s->spi_sr &= ~SPI_SR_OVERRUN;
            }
            if (value & SPI_SR_UNDERRUN) {
                s->spi_sr &= ~SPI_SR_UNDERRUN;
            }
            g233_spi_update_irq(s);
            return ;
        case SPI_DR:
            s->spi_dr = value;
            g233_spi_transfer(s);
            return ;
        case SPI_CSCTRL:
            s->spi_csctrl = value;
            g233_spi_update_cs(s);
            return ;
    }
}

static uint64_t g233_spi_read(void *opaque, hwaddr offset, unsigned int size)
{
    G233SPIState *s = opaque;
    switch(offset) {
        case SPI_CR1:
            return s->spi_cr1;
        case SPI_CR2:
            return s->spi_cr2;
        case SPI_SR:
            return s->spi_sr;
        case SPI_DR:
            if ((s->spi_sr & SPI_SR_TXE) == 0) {
                s->spi_sr |= SPI_SR_UNDERRUN;
            } else {
                s->spi_sr &= ~SPI_SR_RXNE;
            }

            g233_spi_update_irq(s);

            return s->spi_dr;
        case SPI_CSCTRL:
            return s->spi_csctrl;
        default:
            return 0;
    }
}

static const MemoryRegionOps g233_spi_ops = {
    .read = g233_spi_read,
    .write = g233_spi_write,
    .endianness = DEVICE_NATIVE_ENDIAN
};

static void g233_spi_instance_init(Object *obj) 
{
    G233SPIState *s = G233_SPI(obj);
    DeviceState *ds = DEVICE(obj);

    memory_region_init_io(&s->mmio, obj, &g233_spi_ops,
                          s, TYPE_G233_SPI, 0x400);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->mmio);
    sysbus_init_irq(SYS_BUS_DEVICE(obj), &s->irq);

    for (int i = 0; i < 4; i++) {
        sysbus_init_irq(SYS_BUS_DEVICE(obj), &s->cs_line[i]);
    }

    s->ssi = ssi_create_bus(ds, "ssi");
}

static void g233_spi_reset(DeviceState *dev) {
    G233SPIState *s = G233_SPI(dev);
    s->spi_cr1    = 0x00000000;
    s->spi_cr2    = 0x00000000;
    s->spi_sr     = 0x00000002;
    s->spi_dr     = 0x0000000C;
    s->spi_csctrl = 0x00000000;
    g233_spi_update_cs(s);
}

static void g233_spi_class_init(ObjectClass *oc, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    device_class_set_legacy_reset(dc, g233_spi_reset);
    dc->vmsd = &vmstate_g233_spi;
}

static const TypeInfo g233_spi_info = {
    .name = TYPE_G233_SPI,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(G233SPIState),
    .class_init = g233_spi_class_init,
    .instance_init = g233_spi_instance_init
};

static void g233_spi_register_types(void) 
{
    type_register_static(&g233_spi_info);
}

type_init(g233_spi_register_types)
