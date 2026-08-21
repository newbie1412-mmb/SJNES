#include "Mapper_019.h"
#include "Namco163Audio.h"

Mapper_019::Mapper_019(
    uint8_t prgBanks,
    uint8_t chrBanks
)
    : Mapper(prgBanks, chrBanks)
{
    reset();
}

void Mapper_019::reset()
{
    prg[0] = 0;
    prg[1] = 1;
    prg[2] = 2;

    // FIX LAST BANK
    prg[3] = (nPRGBanks * 2) - 1;

    // 8 slot đầu: pattern table $0000-$1FFF
    for (int i = 0; i < 8; i++)
        chr[i] = i;
    chr[8] = 0xE0;
    chr[9] = 0xE1;
    chr[10] = 0xE0;
    chr[11] = 0xE1;
    UpdateNametableMirror();
    irq_counter = 0;
    irq_enable = false;
    irq_active = false;

    reg_E800 = 0;

    n163.Reset();
    n163.SetVolume(0.35f);
    n163.SetAccurateMultiplex(false);  
}

bool Mapper_019::cpuReadRegister(uint16_t addr, uint8_t& data)
{
    if (addr >= 0x4800 && addr <= 0x4FFF)
    {
        data = n163.ReadDataPort();
        return true;
    }

    return false;
}

bool Mapper_019::cpuMapRead(
    uint16_t addr,
    uint32_t& mapped
)
{
    if (addr >= 0x8000)
    {
        int slot =
            (addr - 0x8000) >> 13;

        uint8_t bank =
            (slot == 3)
            ?
            ((nPRGBanks * 2) - 1)
            :
            prg[slot];

        mapped =
            bank * 0x2000
            +
            (addr & 0x1FFF);

        return true;
    }

    return false;
}

bool Mapper_019::cpuMapWrite(uint16_t addr, uint32_t& mapped, uint8_t data)
{
    (void)(mapped);

    // $4800-$4FFF: N163 data port
    // Ghi vào 128 byte sound RAM bên trong chip
    if (addr >= 0x4800 && addr <= 0x4FFF)
    {
        n163.WriteDataPort(data);
        return true;
    }

    // $5000-$57FF: IRQ low byte
    if (addr >= 0x5000 && addr <= 0x57FF)
    {
        irq_counter = (irq_counter & 0x7F00) | data;

        // Ghi IRQ register sẽ acknowledge IRQ
        irq_active = false;

        return true;
    }

    // $5800-$5FFF: IRQ high byte + enable
    if (addr >= 0x5800 && addr <= 0x5FFF)
    {
        irq_counter = (irq_counter & 0x00FF) | ((data & 0x7F) << 8);
        irq_enable = (data & 0x80) != 0;

        // Ghi IRQ register sẽ acknowledge IRQ
        irq_active = false;
        return true;
    }

    // $8000-$DFFF: CHR bank / nametable bank
    if (addr >= 0x8000 && addr <= 0xDFFF)
    {
        int slot = (addr - 0x8000) >> 11;

        if (slot >= 0 && slot < 12)
        {
            chr[slot] = data;
        }

        return true;
    }

    // $E000-$E7FF: PRG bank 0 + N163 sound enable
    if (addr >= 0xE000 && addr <= 0xE7FF)
    {
        // Bit 6 điều khiển tắt/bật N163 audio
        n163.WriteSoundEnable(data);

        // PRG bank $8000-$9FFF
        prg[0] = (data & 0x3F) % (nPRGBanks * 2);

        return true;
    }

    // $E800-$EFFF: PRG bank 1
    if (addr >= 0xE800 && addr <= 0xEFFF)
    {
        prg[1] = (data & 0x3F) % (nPRGBanks * 2);
        reg_E800 = data;
        return true;
    }

    // $F000-$F7FF: PRG bank 2
    if (addr >= 0xF000 && addr <= 0xF7FF)
    {
        prg[2] = (data & 0x3F) % (nPRGBanks * 2);
        return true;
    }

    // $F800-$FFFF: N163 address port
    // bit 0-6 = địa chỉ sound RAM
    // bit 7 = auto increment
    if (addr >= 0xF800 && addr <= 0xFFFF)
    {
        n163.WriteAddressPort(data);
        return true;
    }

    return false;
}

uint32_t Mapper_019::GetCiramBase() const
{
    // Cartridge.cpp của anh yêu đã resize thêm 2048 byte vào cuối vCHRMemory
    // khi nMapperID == 19, nên base này trỏ tới 2KB CIRAM ảo đó.
    if (nCHRBanks > 0)
        return uint32_t(nCHRBanks) * 8192;

    // Nếu CHR-RAM, 8KB đầu là CHR-RAM, 2KB sau là CIRAM ảo
    return 8192;
}

bool Mapper_019::BankUsesCiram(int slot, uint8_t bank) const
{
    if (bank < 0xE0)
        return false;

    // Slot 0-3: PPU $0000-$0FFF
    // Nếu reg_E800 bit 6 = 0 thì bank $E0-$FF trỏ CIRAM
    // Nếu bit 6 = 1 thì $E0-$FF vẫn là CHR-ROM
    if (slot >= 0 && slot <= 3)
        return (reg_E800 & 0x40) == 0;

    // Slot 4-7: PPU $1000-$1FFF
    // Nếu reg_E800 bit 7 = 0 thì bank $E0-$FF trỏ CIRAM
    // Nếu bit 7 = 1 thì $E0-$FF vẫn là CHR-ROM
    if (slot >= 4 && slot <= 7)
        return (reg_E800 & 0x80) == 0;

    // Slot 8-11: PPU $2000-$2FFF nametable windows
    // Với nametable, $E0-$FF là CIRAM A/B
    if (slot >= 8 && slot <= 11)
        return true;

    return false;
}

void Mapper_019::UpdateNametableMirror()
{
    // Chỉ xử lý kiểu CIRAM $E0/$E1.
    // Nếu game dùng bank < $E0 cho nametable thì đây là CHR-ROM nametable,
    // tạm thời không đổi mirroring.
    if (chr[8] < 0xE0 || chr[9] < 0xE0 || chr[10] < 0xE0 || chr[11] < 0xE0)
        return;

    int nt0 = chr[8] & 0x01;   // $2000
    int nt1 = chr[9] & 0x01;   // $2400
    int nt2 = chr[10] & 0x01;  // $2800
    int nt3 = chr[11] & 0x01;  // $2C00

    // Vertical: A B A B
    if (nt0 == 0 && nt1 == 1 && nt2 == 0 && nt3 == 1)
    {
        mirroring_mode = VERTICAL;
        return;
    }

    // Horizontal: A A B B
    if (nt0 == 0 && nt1 == 0 && nt2 == 1 && nt3 == 1)
    {
        mirroring_mode = HORIZONTAL;
        return;
    }

    // One screen low: A A A A
    if (nt0 == 0 && nt1 == 0 && nt2 == 0 && nt3 == 0)
    {
        mirroring_mode = ONESCREEN_LO;
        return;
    }

    // One screen high: B B B B
    if (nt0 == 1 && nt1 == 1 && nt2 == 1 && nt3 == 1)
    {
        mirroring_mode = ONESCREEN_HI;
        return;
    }

    // Trường hợp lạ hơn: A B B A hoặc kiểu custom.
    // Enum MIRROR hiện tại không biểu diễn được.
    mirroring_mode = HARDWARE;
}

bool Mapper_019::MapPpuAddress(uint16_t addr, uint32_t& mapped, bool forWrite) const
{
    addr &= 0x3FFF;

    // $3000-$3EFF mirror về $2000-$2EFF
    if (addr >= 0x3000 && addr < 0x3F00)
        addr -= 0x1000;

    // Palette để PPU tự xử lý
    if (addr >= 0x3F00)
        return false;

    // $0000-$2FFF chia thành 12 slot 1KB
    if (addr < 0x3000)
    {
        int slot = addr >> 10;          // 0..11
        uint16_t offset = addr & 0x03FF;

        if (slot < 0 || slot >= 12)
            return false;

        uint8_t bank = chr[slot];

        // Nametable slot 8..11 + bank $E0-$FF:
        // dùng CIRAM nội bộ của PPU như mirroring bình thường.
        // Nametable $2000-$2FFF
        if (slot >= 8 && slot <= 11)
        {
            // $E0-$FF = CIRAM nội bộ
            if (bank >= 0xE0)
            {
                return false;
            }

            // Tạm thời tắt CHR-ROM nametable để tránh Megami bị nền đen/sai tile.
            // Nếu bật true thì bank < $E0 sẽ map sang CHR-ROM.
            if (!enableChrRomNametable)
            {
                return false;
            }
        }

        // Pattern table slot 0..7 + bank $E0-$FF:
        // có thể map vào CIRAM ảo.
        if (BankUsesCiram(slot, bank))
        {
            uint32_t ciramBase = GetCiramBase();
            mapped = ciramBase + uint32_t(bank & 0x01) * 0x400 + offset;
            return true;
        }

        // Bank $00-$DF: map CHR-ROM / CHR-RAM
        if (nCHRBanks > 0)
        {
            uint32_t total1KBanks = uint32_t(nCHRBanks) * 8;
            mapped = uint32_t(bank % total1KBanks) * 0x400 + offset;

            // CHR-ROM không cho ghi
            if (forWrite)
                mapped = WRITE_SINK;

            return true;
        }
        else
        {
            // CHR-RAM 8KB
            mapped = uint32_t(bank % 8) * 0x400 + offset;
            return true;
        }
    }

    return false;
}

bool Mapper_019::ppuMapRead(uint16_t addr, uint32_t& mapped)
{
    return MapPpuAddress(addr, mapped, false);
}

bool Mapper_019::ppuMapWrite(uint16_t addr, uint32_t& mapped)
{
    return MapPpuAddress(addr, mapped, true);
}

void Mapper_019::clock()
{
    if (!irq_enable)
        return;

    if (irq_active)
        return;

    if (irq_counter < 0x7FFF)
    {
        irq_counter++;

        if (irq_counter >= 0x7FFF)
        {
            irq_counter = 0x7FFF;
            irq_active = true;
        }
    }
}

bool Mapper_019::irqState() {
    return irq_active;
}

void Mapper_019::irqClear() {
    irq_active = false;
}

void Mapper_019::ClockCpu(int cycles)
{
    n163.ClockCpu(cycles);
}

float Mapper_019::GetExpansionAudio()
{
    return n163.GetSample();
}

void Mapper_019::irqStep()
{
    // IRQ Mapper 19
    clock();

    // N163 audio chạy theo từng CPU cycle
    n163.ClockCpu(1);
}
void Mapper_019::GetN163DebugChannels(
    float& ch1, float& ch2, float& ch3, float& ch4,
    float& ch5, float& ch6, float& ch7, float& ch8
)
{
    float out[8]{};
    n163.GetDebugChannels(out);

    ch1 = out[0];
    ch2 = out[1];
    ch3 = out[2];
    ch4 = out[3];
    ch5 = out[4];
    ch6 = out[5];
    ch7 = out[6];
    ch8 = out[7];
}

void Mapper_019::GetN163DebugPeriods(
    float& ch1, float& ch2, float& ch3, float& ch4,
    float& ch5, float& ch6, float& ch7, float& ch8
)
{
    float out[8]{};
    n163.GetDebugPeriods(out);

    ch1 = out[0];
    ch2 = out[1];
    ch3 = out[2];
    ch4 = out[3];
    ch5 = out[4];
    ch6 = out[5];
    ch7 = out[6];
    ch8 = out[7];
}