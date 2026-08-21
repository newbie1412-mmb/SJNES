#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <map>
#include "BinaryIO.h"
class Bus;

class CPU6502
{
public:
    CPU6502();
    ~CPU6502();
    int irqHandledCount = 0;
    int irqBlockedCount = 0;

public:
    // Các thanh ghi cốt lõi
    uint8_t  a = 0x00;
    uint8_t  x = 0x00;
    uint8_t  y = 0x00;
    uint8_t  stkp = 0x00;
    uint16_t pc = 0x0000;
    uint8_t  status = 0x00;

    void reset();
    void irq();
    void nmi();
    void clock();

    enum IRQSource : uint8_t
    {
        IRQ_EXTERNAL = 1 << 0,
        IRQ_APU = 1 << 1,
        IRQ_DMC = 1 << 2,
    };
    void SetIrqSource(uint8_t source);
    void ClearIrqSource(uint8_t source);
    bool IsIrqActive() const;

    void ConnectBus(Bus* n) { bus = n; }

    uint8_t fetch();
    std::map<uint16_t, std::string> disassemble(uint16_t nStart, uint16_t nStop);
    bool complete();
    void SaveState(BinaryWriter& out) const;
    void LoadState(BinaryReader& in);
public:
    uint16_t  cycles = 0;
    enum FLAGS6502
    {
        C = (1 << 0), Z = (1 << 1), I = (1 << 2), D = (1 << 3),
        B = (1 << 4), U = (1 << 5), V = (1 << 6), N = (1 << 7),
    };

private:
    uint8_t GetFlag(FLAGS6502 f);
    void    SetFlag(FLAGS6502 f, bool v);
    bool nmi_pending = false;
    bool irq_pending = false;
    uint8_t irq_sources = 0;
    uint8_t  fetched = 0x00;
    uint16_t temp = 0x0000;
    uint16_t addr_abs = 0x0000;
    uint16_t addr_rel = 0x0000;
    uint8_t  opcode = 0x00;
    uint32_t clock_count = 0;

    // ==== State machine cho cycle-accurate execution ====
    int instr_cycle = 0;
    uint8_t  addr_lo = 0;
    uint16_t base_addr = 0;
    uint16_t eff_addr = 0;
    uint8_t  rmw_old_val = 0;
    bool     page_crossed = false;
    bool     branch_taken = false;

    // Trạng thái riêng cho NMI/IRQ - KHÔNG dùng chung với `opcode` vì 0xFE/0xFF
    // là các opcode THẬT (INC abs,X / ISC abs,X) sẽ gây xung đột nghiêm trọng.
    enum class SpecialMode : uint8_t { NONE_MODE, NMI_MODE, IRQ_MODE };
    SpecialMode specialMode = SpecialMode::NONE_MODE;

    // CLI/SEI/PLP trì hoãn hiệu lực IRQ-inhibition đúng 1 lệnh theo chuẩn 6502 thật.
    // irq_disable_shadow là giá trị "I" THỰC SỰ được dùng để xét IRQ (trễ hơn GetFlag(I) thật
    // đúng 1 nhịp bất cứ khi nào lệnh vừa chạy là CLI/SEI/PLP).
    uint8_t irq_disable_shadow = 1;
    bool    owe_shadow_sync = false;

    // Dùng RIÊNG cho rule "nhánh rẽ taken không crosspage bỏ qua IRQ check ở cycle cuối"
    // (không liên quan tới CLI/SEI/PLP, không dùng chung biến).
    bool     suppress_irq_check_this_instruction = false;

    enum class AddrClass : uint8_t
    {
        IMP_ACC, IMM_, ZPG_, ZPX_, ZPY_, ABS_, ABX_, ABY_,
        IZX_, IZY_, REL_, IND_, NONE_
    };
    enum class OpClass : uint8_t
    {
        READ, WRITE, RMW, BRANCH, JUMP, JSR_OP, RTS_OP, RTI_OP, BRK_OP,
        PHA_OP, PHP_OP, PLA_OP, PLP_OP, IMPLIED_ONLY
    };

    struct INSTRUCTION2
    {
        AddrClass amode = AddrClass::NONE_;
        OpClass   otype = OpClass::IMPLIED_ONLY;
        uint8_t(CPU6502::* operate)(void) = nullptr;
    };
    std::vector<INSTRUCTION2> lookup2;
    void BuildLookup2();

    void StepInstructionCycle();
    void FinishInstruction();

    Bus* bus = nullptr;
    uint8_t read(uint16_t a);
    void    write(uint16_t a, uint8_t d);

    struct INSTRUCTION
    {
        std::string name;
        uint8_t(CPU6502::* operate)(void) = nullptr;
        uint8_t(CPU6502::* addrmode)(void) = nullptr;
        uint8_t cycles = 0;
    };

    std::vector<INSTRUCTION> lookup;

private:
    uint8_t IMP(); uint8_t IMM(); uint8_t ZPG(); uint8_t ZPX();
    uint8_t ZPY(); uint8_t REL(); uint8_t ABS(); uint8_t ABX();
    uint8_t ABY(); uint8_t IND(); uint8_t IZX(); uint8_t IZY();
    uint8_t ADC(); uint8_t AND(); uint8_t ASL(); uint8_t BCC();
    uint8_t BCS(); uint8_t BEQ(); uint8_t BIT(); uint8_t BMI();
    uint8_t BNE(); uint8_t BPL(); uint8_t BRK(); uint8_t BVC();
    uint8_t BVS(); uint8_t CLC(); uint8_t CLD(); uint8_t CLI();
    uint8_t CLV(); uint8_t CMP(); uint8_t CPX(); uint8_t CPY();
    uint8_t DEC(); uint8_t DEX(); uint8_t DEY(); uint8_t EOR();
    uint8_t INC(); uint8_t INX(); uint8_t INY(); uint8_t JMP();
    uint8_t JSR(); uint8_t LDA(); uint8_t LDX(); uint8_t LDY();
    uint8_t LSR(); uint8_t NOP(); uint8_t ORA(); uint8_t PHA();
    uint8_t PHP(); uint8_t PLA(); uint8_t PLP(); uint8_t ROL();
    uint8_t ROR(); uint8_t RTI(); uint8_t RTS(); uint8_t SBC();
    uint8_t SEC(); uint8_t SED(); uint8_t SEI(); uint8_t STA();
    uint8_t STX(); uint8_t STY(); uint8_t TAX(); uint8_t TAY();
    uint8_t TSX(); uint8_t TXA(); uint8_t TXS(); uint8_t TYA();
    uint8_t XXX(); uint8_t LAX();
    uint8_t SAX();
    uint8_t DCP();
    uint8_t ISC();
    uint8_t SLO();
    uint8_t RLA();
    uint8_t SRE();
    uint8_t RRA();
};

std::string hex(uint32_t n, uint8_t d);