// ============================================================================
// CPU6502_CycleAccurate.inc
// Include file này vào CUỐI CPU6502.cpp (sau khi lookup[] đã được build trong
// constructor). Thay thế nội dung hàm clock() cũ bằng bản cycle-accurate.
// ============================================================================

// ---- Bước 1: build bảng phân loại song song với lookup[] ----
// So sánh con trỏ hàm addrmode/operate đã có sẵn để tự động phân loại,
// khỏi phải gõ tay 151 dòng.
void CPU6502::BuildLookup2()
{
    lookup2.clear();
    lookup2.resize(lookup.size());

    for (size_t i = 0; i < lookup.size(); i++)
    {
        auto& L  = lookup[i];
        auto& L2 = lookup2[i];
        L2.operate = L.operate;

        // ---- Phân loại Addressing Mode theo con trỏ addrmode ----
        if      (L.addrmode == &CPU6502::IMP) L2.amode = AddrClass::IMP_ACC;
        else if (L.addrmode == &CPU6502::IMM) L2.amode = AddrClass::IMM_;
        else if (L.addrmode == &CPU6502::ZPG) L2.amode = AddrClass::ZPG_;
        else if (L.addrmode == &CPU6502::ZPX) L2.amode = AddrClass::ZPX_;
        else if (L.addrmode == &CPU6502::ZPY) L2.amode = AddrClass::ZPY_;
        else if (L.addrmode == &CPU6502::ABS) L2.amode = AddrClass::ABS_;
        else if (L.addrmode == &CPU6502::ABX) L2.amode = AddrClass::ABX_;
        else if (L.addrmode == &CPU6502::ABY) L2.amode = AddrClass::ABY_;
        else if (L.addrmode == &CPU6502::IZX) L2.amode = AddrClass::IZX_;
        else if (L.addrmode == &CPU6502::IZY) L2.amode = AddrClass::IZY_;
        else if (L.addrmode == &CPU6502::REL) L2.amode = AddrClass::REL_;
        else if (L.addrmode == &CPU6502::IND) L2.amode = AddrClass::IND_;
        else                                   L2.amode = AddrClass::NONE_;

        // ---- Phân loại Operation Type theo tên lệnh ----
        const std::string& name = L.name;

        if (name == "JSR") { L2.otype = OpClass::JSR_OP; continue; }
        if (name == "RTS") { L2.otype = OpClass::RTS_OP; continue; }
        if (name == "RTI") { L2.otype = OpClass::RTI_OP; continue; }
        if (name == "BRK") { L2.otype = OpClass::BRK_OP; continue; }
        if (name == "PHA" || name == "PHP") { L2.otype = OpClass::PHA_OP; continue; }
        if (name == "PLA" || name == "PLP") { L2.otype = OpClass::PLA_OP; continue; }
        if (name == "JMP") { L2.otype = OpClass::JUMP; continue; }
        if (name[0] == 'B' && name != "BIT" && name != "BRK") // BCC,BCS,BEQ,BMI,BNE,BPL,BVC,BVS
        {
            L2.otype = OpClass::BRANCH; continue;
        }

        // Write-only: STA/STX/STY/SAX
        if (name == "STA" || name == "STX" || name == "STY" || name == "SAX")
        {
            L2.otype = OpClass::WRITE; continue;
        }

        // Read-Modify-Write (kể cả illegal opcode kết hợp): ASL,LSR,ROL,ROR,INC,DEC,
        // SLO,SRE,RLA,RRA,DCP,ISC đều đọc-sửa-ghi lại đúng 1 địa chỉ bộ nhớ (trừ dạng IMP/Accumulator)
        static const std::vector<std::string> rmwNames =
        {
            "ASL","LSR","ROL","ROR","INC","DEC","SLO","SRE","RLA","RRA","DCP","ISC"
        };
        bool isRmwName = false;
        for (auto& n : rmwNames) if (name == n) { isRmwName = true; break; }

        if (isRmwName)
        {
            // Accumulator mode (ASL A, ROL A...) không đụng bus ngoài -> coi như IMPLIED
            if (L2.amode == AddrClass::IMP_ACC)
                L2.otype = OpClass::IMPLIED_ONLY;
            else
                L2.otype = OpClass::RMW;
            continue;
        }

        if (L2.amode == AddrClass::IMP_ACC || L2.amode == AddrClass::NONE_)
        {
            L2.otype = OpClass::IMPLIED_ONLY;
            continue;
        }

        // Mặc định: các lệnh còn lại (LDA,ADC,AND,CMP,LAX,NOP có addr...) là READ
        L2.otype = OpClass::READ;
    }
}

// ---- Bước 2: dispatcher clock() mới, cycle-accurate ----
// GHI CHÚ: hàm clock() cũ trong CPU6502.cpp cần được XÓA/COMMENT, thay bằng hàm này.
void CPU6502::clock()
{
    // ---- Ranh giới lệnh: instr_cycle == 0 nghĩa là chuẩn bị fetch opcode mới ----
    if (instr_cycle == 0 && cycles == 0)
    {
        if (nmi_pending)
        {
            nmi_pending = false;
            instr_cycle = 1;
            specialMode = SpecialMode::NMI_MODE;
            cycles = 7;
            return;
        }
        else if (IsIrqActive() && irq_disable_shadow == 0 && !suppress_irq_check_this_instruction)
        {
            // Đã dùng xong shadow (giá trị trễ) cho lần xét này - giờ đồng bộ nếu đang nợ
            if (owe_shadow_sync) { irq_disable_shadow = GetFlag(I); owe_shadow_sync = false; }

            if (irq_sources == 0) irq_pending = false;
            irqHandledCount++;
            instr_cycle = 1;
            specialMode = SpecialMode::IRQ_MODE;
            cycles = 7;
            return;
        }
        else
        {
            // Cờ trì hoãn (rule nhánh rẽ) chỉ có hiệu lực cho ĐÚNG 1 lần kiểm tra IRQ kế tiếp
            suppress_irq_check_this_instruction = false;

            // Đã dùng xong shadow (giá trị trễ) cho lần xét này - giờ đồng bộ nếu đang nợ
            if (owe_shadow_sync) { irq_disable_shadow = GetFlag(I); owe_shadow_sync = false; }

            if (IsIrqActive() && irq_disable_shadow != 0) irqBlockedCount++;

            specialMode = SpecialMode::NONE_MODE;
            opcode = read(pc);
            pc++;
            SetFlag(U, true);
            instr_cycle = 1; // cycle 1 = vừa fetch xong opcode, cycle kế tiếp bắt đầu addressing
            return; // KHÔNG làm gì thêm trong cùng 1 lần gọi clock() -> đúng timing
        }
    }

    // ---- Xử lý NMI/IRQ theo đúng 7 cycle (dummy read, push PC, push status, load vector) ----
    if (specialMode != SpecialMode::NONE_MODE)
    {
        bool isNmi = (specialMode == SpecialMode::NMI_MODE);
        switch (instr_cycle)
        {
            case 1: read(pc); break;                      // cycle 2: dummy read
            case 2: write(0x0100 + stkp, (pc >> 8) & 0xFF); stkp--; break;
            case 3: write(0x0100 + stkp, pc & 0xFF); stkp--; break;
            case 4:
            {
                uint8_t pushedStatus = (status & ~B) | U;
                write(0x0100 + stkp, pushedStatus); stkp--;
                SetFlag(I, true);
                SetFlag(U, true);
                // QUAN TRỌNG: đồng bộ shadow NGAY LẬP TỨC khi phần cứng tự set I=1 lúc vào ngắt,
                // nếu không shadow sẽ vẫn "tưởng" ngắt đang cho phép -> lồng ngắt vô hạn.
                irq_disable_shadow = 1;
                owe_shadow_sync = false;
                break;
            }
            case 5: temp = read(isNmi ? 0xFFFA : 0xFFFE); break; // lo byte vector
            case 6:
            {
                uint16_t hi = read(isNmi ? 0xFFFB : 0xFFFF);
                pc = (hi << 8) | temp;
                instr_cycle = 0;
                cycles = 0;
                specialMode = SpecialMode::NONE_MODE;
                clock_count++;
                return;
            }
        }
        instr_cycle++;
        clock_count++;
        cycles--;
        return;
    }

    // ---- Lệnh bình thường: chạy state machine theo addressing mode ----
    StepInstructionCycle();
    clock_count++;
    if (cycles > 0) cycles--;
}

// ---- Bước 3: state machine chính cho từng addressing mode ----
// instr_cycle=1 là cycle NGAY SAU khi fetch opcode (đã pc++).
// Các case dưới đánh số theo "cycle thứ mấy của lệnh" (T2, T3, T4... theo tài liệu 6502).
void CPU6502::StepInstructionCycle()
{
    auto& L2 = lookup2[opcode];

    // ===== Các lệnh đặc biệt (stack/control-flow) — xử lý TRƯỚC, bỏ qua addressing mode bảng gốc =====
    switch (L2.otype)
    {
    case OpClass::JSR_OP:
    {
        if (instr_cycle == 1) { addr_lo = read(pc); pc++; }
        else if (instr_cycle == 2) { read(0x0100 + stkp); /* internal, dummy */ }
        else if (instr_cycle == 3) { write(0x0100 + stkp, (pc >> 8) & 0xFF); stkp--; }
        else if (instr_cycle == 4) { write(0x0100 + stkp, pc & 0xFF); stkp--; }
        else if (instr_cycle == 5)
        {
            uint16_t hi = read(pc);
            pc = (hi << 8) | addr_lo;
            instr_cycle = 0; cycles = 0;
            irq_disable_shadow = GetFlag(I); owe_shadow_sync = false;
            return;
        }
        instr_cycle++;
        return;
    }
    case OpClass::RTS_OP:
    {
        if (instr_cycle == 1) { read(pc); }
        else if (instr_cycle == 2) { read(0x0100 + stkp); }
        else if (instr_cycle == 3) { stkp++; temp = read(0x0100 + stkp); }
        else if (instr_cycle == 4) { stkp++; uint16_t hi = read(0x0100 + stkp); pc = (hi << 8) | temp; }
        else if (instr_cycle == 5)
        {
            pc++;
            instr_cycle = 0; cycles = 0;
            irq_disable_shadow = GetFlag(I); owe_shadow_sync = false;
            return;
        }
        instr_cycle++;
        return;
    }
    case OpClass::RTI_OP:
    {
        if (instr_cycle == 1) { read(pc); }
        else if (instr_cycle == 2) { read(0x0100 + stkp); }
        else if (instr_cycle == 3) { stkp++; status = read(0x0100 + stkp); status &= ~B; status |= U; }
        else if (instr_cycle == 4) { stkp++; temp = read(0x0100 + stkp); }
        else if (instr_cycle == 5)
        {
            stkp++;
            uint16_t hi = read(0x0100 + stkp);
            pc = (hi << 8) | temp;
            instr_cycle = 0; cycles = 0;
            // RTI ảnh hưởng IRQ-inhibition NGAY LẬP TỨC, không trễ như CLI/SEI/PLP.
            irq_disable_shadow = GetFlag(I); owe_shadow_sync = false;
            return;
        }
        instr_cycle++;
        return;
    }
    case OpClass::BRK_OP:
    {
        if (instr_cycle == 1) { read(pc); pc++; }
        else if (instr_cycle == 2) { write(0x0100 + stkp, (pc >> 8) & 0xFF); stkp--; }
        else if (instr_cycle == 3) { write(0x0100 + stkp, pc & 0xFF); stkp--; }
        else if (instr_cycle == 4) { write(0x0100 + stkp, status | B | U); stkp--; SetFlag(I, true); }
        else if (instr_cycle == 5)
        {
            // === NMI HIJACKING ===
            // Nếu NMI xảy ra trước thời điểm này (trước khi đọc byte thấp của vector),
            // phần cứng thật sẽ "cướp" chuỗi BRK: PC/status vẫn được push như BRK bình
            // thường (đã làm ở cycle 2-4), nhưng vector nạp vào PC sẽ là vector NMI
            // ($FFFA/$FFFB) thay vì vector BRK/IRQ ($FFFE/$FFFF). NMI coi như đã được
            // xử lý xong bởi lần hijack này nên phải xoá nmi_pending.
            if (nmi_pending)
            {
                nmi_pending = false;
                brk_hijacked_by_nmi = true;
                temp = read(0xFFFA);
            }
            else
            {
                temp = read(0xFFFE);
            }
        }
        else if (instr_cycle == 6)
        {
            uint16_t hi = read(brk_hijacked_by_nmi ? 0xFFFB : 0xFFFF);
            pc = (hi << 8) | temp;
            instr_cycle = 0; cycles = 0;
            irq_disable_shadow = GetFlag(I); owe_shadow_sync = false;
            brk_hijacked_by_nmi = false;
            return;
        }
        instr_cycle++;
        return;
    }
    case OpClass::PHA_OP:
    {
        auto& L = lookup[opcode];
        if (instr_cycle == 1) { read(pc); }
        else if (instr_cycle == 2)
        {
            uint8_t v = (L.name == "PHA") ? a : (status | B | U);
            write(0x0100 + stkp, v); stkp--;
            instr_cycle = 0; cycles = 0;
            irq_disable_shadow = GetFlag(I); owe_shadow_sync = false;
            return;
        }
        instr_cycle++;
        return;
    }
    case OpClass::PLA_OP:
    {
        auto& L = lookup[opcode];
        if (instr_cycle == 1) { read(pc); }
        else if (instr_cycle == 2) { read(0x0100 + stkp); }
        else if (instr_cycle == 3)
        {
            stkp++;
            uint8_t v = read(0x0100 + stkp);
            if (L.name == "PLA")
            {
                a = v; SetFlag(Z, a == 0); SetFlag(N, a & 0x80);
                irq_disable_shadow = GetFlag(I); owe_shadow_sync = false;
            }
            else
            {
                // PLP cũng nằm trong nhóm trì hoãn (CLI/SEI/PLP) theo readme.
                status = v; status &= ~B; status |= U;
                owe_shadow_sync = true;
            }
            instr_cycle = 0; cycles = 0;
            return;
        }
        instr_cycle++;
        return;
    }
    default: break; // rơi xuống switch theo addressing mode bên dưới
    }

    switch (L2.amode)
    {
    // ================= IMPLIED / ACCUMULATOR =================
    // T2: dummy read tại PC hiện tại (không tăng PC), rồi execute luôn.
    case AddrClass::IMP_ACC:
    {
        if (instr_cycle == 1)
        {
            read(pc); // dummy read, không dùng kết quả
            fetched = a; // cho các lệnh ASL A/ROL A... dùng accumulator
            FinishInstruction();
            return;
        }
        break;
    }

    // ================= IMMEDIATE =================
    // T2: đọc operand ngay sau opcode, execute luôn.
    case AddrClass::IMM_:
    {
        if (instr_cycle == 1)
        {
            fetched = read(pc); pc++;
            FinishInstruction();
            return;
        }
        break;
    }

    // ================= ZERO PAGE =================
    // Read: T2 đọc addr, T3 đọc giá trị + execute
    // Write: T2 đọc addr, T3 ghi giá trị
    // RMW:   T2 đọc addr, T3 đọc old, T4 ghi old (dummy), T5 ghi new
    case AddrClass::ZPG_:
    {
        if (instr_cycle == 1) { addr_lo = read(pc); pc++; eff_addr = addr_lo; }
        else if (instr_cycle == 2)
        {
            if (L2.otype == OpClass::WRITE) { FinishInstruction(); return; }
            rmw_old_val = read(eff_addr);
            fetched = rmw_old_val;
            if (L2.otype == OpClass::READ) { FinishInstruction(); return; }
        }
        else if (instr_cycle == 3 && L2.otype == OpClass::RMW)
        {
            write(eff_addr, rmw_old_val); // dummy write giá trị cũ (đúng hardware thật)
        }
        else if (instr_cycle == 4 && L2.otype == OpClass::RMW)
        {
            FinishInstruction();
            return;
        }
        break;
    }

    // ================= ZERO PAGE,X / ZERO PAGE,Y =================
    // T2 đọc addr base, T3 dummy read tại addr base (cộng index), rồi giống ZPG từ đó
    case AddrClass::ZPX_:
    case AddrClass::ZPY_:
    {
        uint8_t idx = (L2.amode == AddrClass::ZPX_) ? x : y;
        if (instr_cycle == 1) { addr_lo = read(pc); pc++; }
        else if (instr_cycle == 2)
        {
            read(addr_lo); // dummy read tại địa chỉ chưa cộng index
            eff_addr = (uint8_t)(addr_lo + idx); // wrap trong trang 0
        }
        else if (instr_cycle == 3)
        {
            if (L2.otype == OpClass::WRITE) { FinishInstruction(); return; }
            rmw_old_val = read(eff_addr);
            fetched = rmw_old_val;
            if (L2.otype == OpClass::READ) { FinishInstruction(); return; }
        }
        else if (instr_cycle == 4 && L2.otype == OpClass::RMW)
        {
            write(eff_addr, rmw_old_val);
        }
        else if (instr_cycle == 5 && L2.otype == OpClass::RMW)
        {
            FinishInstruction();
            return;
        }
        break;
    }

    // ================= ABSOLUTE =================
    case AddrClass::ABS_:
    {
        if (instr_cycle == 1) { addr_lo = read(pc); pc++; }
        else if (instr_cycle == 2)
        {
            uint16_t hi = read(pc); pc++;
            eff_addr = (hi << 8) | addr_lo;
            if (L2.otype == OpClass::JUMP) { pc = eff_addr; FinishInstruction(); return; }
        }
        else if (instr_cycle == 3)
        {
            if (L2.otype == OpClass::WRITE) { FinishInstruction(); return; }
            rmw_old_val = read(eff_addr);
            fetched = rmw_old_val;
            if (L2.otype == OpClass::READ) { FinishInstruction(); return; }
        }
        else if (instr_cycle == 4 && L2.otype == OpClass::RMW)
        {
            write(eff_addr, rmw_old_val);
        }
        else if (instr_cycle == 5 && L2.otype == OpClass::RMW)
        {
            FinishInstruction();
            return;
        }
        break;
    }

    // ================= ABSOLUTE,X / ABSOLUTE,Y =================
    // Read: nếu KHÔNG crosspage -> 4 cycle; nếu crosspage -> 5 cycle (dummy read địa chỉ sai trang trước)
    // Write & RMW: LUÔN có dummy read tại địa chỉ chưa fix trang (giống crosspage timing), bất kể có crosspage thật hay không.
    case AddrClass::ABX_:
    case AddrClass::ABY_:
    {
        uint8_t idx = (L2.amode == AddrClass::ABX_) ? x : y;
        if (instr_cycle == 1) { addr_lo = read(pc); pc++; }
        else if (instr_cycle == 2)
        {
            uint16_t hi = read(pc); pc++;
            base_addr = (hi << 8) | addr_lo;
            uint16_t lo_sum = addr_lo + idx;
            page_crossed = (lo_sum > 0xFF);
            // Địa chỉ "sai trang" tạm thời dùng để dummy-read nếu cần
            eff_addr = (base_addr & 0xFF00) | (lo_sum & 0xFF);
        }
        else if (instr_cycle == 3)
        {
            if (L2.otype == OpClass::READ)
            {
                if (page_crossed)
                {
                    read(eff_addr); // dummy read tại địa chỉ sai trang
                    eff_addr = base_addr + idx; // sửa lại đúng
                }
                else
                {
                    eff_addr = base_addr + idx; // không crosspage, eff_addr đã đúng luôn
                    fetched = read(eff_addr);
                    FinishInstruction();
                    return;
                }
            }
            else
            {
                // WRITE / RMW: luôn dummy-read tại địa chỉ chưa fix trang
                read(eff_addr);
                eff_addr = base_addr + idx;
                if (L2.otype == OpClass::WRITE) { /* chờ cycle sau ghi thật */ }
            }
        }
        else if (instr_cycle == 4)
        {
            if (L2.otype == OpClass::READ) // chỉ tới đây khi có crosspage
            {
                fetched = read(eff_addr);
                FinishInstruction();
                return;
            }
            else if (L2.otype == OpClass::WRITE)
            {
                FinishInstruction(); return;
            }
            else // RMW
            {
                rmw_old_val = read(eff_addr);
                fetched = rmw_old_val;
            }
        }
        else if (instr_cycle == 5 && L2.otype == OpClass::RMW)
        {
            write(eff_addr, rmw_old_val);
        }
        else if (instr_cycle == 6 && L2.otype == OpClass::RMW)
        {
            FinishInstruction();
            return;
        }
        break;
    }

    // ================= (Indirect,X) =================
    // T2 đọc ptr, T3 dummy read tại ptr, T4 đọc lo byte tại (ptr+X), T5 đọc hi byte, T6 execute/ghi
    case AddrClass::IZX_:
    {
        if (instr_cycle == 1) { addr_lo = read(pc); pc++; }
        else if (instr_cycle == 2) { read(addr_lo); addr_lo = (uint8_t)(addr_lo + x); }
        else if (instr_cycle == 3) { base_addr = read(addr_lo); }
        else if (instr_cycle == 4)
        {
            uint16_t hi = read((uint8_t)(addr_lo + 1));
            eff_addr = (hi << 8) | base_addr;
        }
        else if (instr_cycle == 5)
        {
            if (L2.otype == OpClass::WRITE) { FinishInstruction(); return; }
            rmw_old_val = read(eff_addr);
            fetched = rmw_old_val;
            if (L2.otype == OpClass::READ) { FinishInstruction(); return; }
        }
        else if (instr_cycle == 6 && L2.otype == OpClass::RMW) { write(eff_addr, rmw_old_val); }
        else if (instr_cycle == 7 && L2.otype == OpClass::RMW) { FinishInstruction(); return; }
        break;
    }

    // ================= (Indirect),Y =================
    case AddrClass::IZY_:
    {
        if (instr_cycle == 1) { addr_lo = read(pc); pc++; }
        else if (instr_cycle == 2) { base_addr = read(addr_lo); }
        else if (instr_cycle == 3)
        {
            uint16_t hi = read((uint8_t)(addr_lo + 1));
            base_addr = (hi << 8) | base_addr;
            uint16_t lo_sum = (base_addr & 0xFF) + y;
            page_crossed = (lo_sum > 0xFF);
            eff_addr = (base_addr & 0xFF00) | (lo_sum & 0xFF);
        }
        else if (instr_cycle == 4)
        {
            if (L2.otype == OpClass::READ)
            {
                if (page_crossed) { read(eff_addr); eff_addr = base_addr + y; }
                else { eff_addr = base_addr + y; fetched = read(eff_addr); FinishInstruction(); return; }
            }
            else
            {
                read(eff_addr);
                eff_addr = base_addr + y;
            }
        }
        else if (instr_cycle == 5)
        {
            if (L2.otype == OpClass::READ) { fetched = read(eff_addr); FinishInstruction(); return; }
            else if (L2.otype == OpClass::WRITE) { FinishInstruction(); return; }
            else { rmw_old_val = read(eff_addr); fetched = rmw_old_val; }
        }
        else if (instr_cycle == 6 && L2.otype == OpClass::RMW) { write(eff_addr, rmw_old_val); }
        else if (instr_cycle == 7 && L2.otype == OpClass::RMW) { FinishInstruction(); return; }
        break;
    }

    // ================= RELATIVE (branch) =================
    // T2: đọc offset. Nếu không nhảy -> xong luôn (2 cycle tổng).
    // Nếu nhảy: T3 dummy read tại PC mới (chưa fix page), +1 cycle nữa nếu crosspage.
    case AddrClass::REL_:
    {
        if (instr_cycle == 1)
        {
            int8_t offset = (int8_t)read(pc); pc++;
            fetched = a; // không dùng, chỉ để hợp lệ
            addr_rel = (uint16_t)(int16_t)offset;

            branch_taken = (this->*L2.operate)() != 0; // gọi BCC/BEQ/... để check điều kiện
            // LƯU Ý: cần sửa các hàm BCC/BEQ/... để KHÔNG tự cộng cycles/pc nữa,
            // chỉ return 1 nếu điều kiện đúng, 0 nếu sai. Xem phần "Cần sửa operate()" bên dưới.

            if (!branch_taken) { FinishInstruction(); return; }
            base_addr = pc;
        }
        else if (instr_cycle == 2)
        {
            uint16_t target = pc + addr_rel;
            read(pc); // dummy read tại PC (chưa nhảy)
            page_crossed = ((target & 0xFF00) != (base_addr & 0xFF00));
            pc = target;
            if (!page_crossed)
            {
                // Rule đặc biệt (Blargg cpu_interrupts #5): nhánh rẽ taken KHÔNG crosspage
                // (chỉ 3 cycle) sẽ bỏ qua việc kiểm tra IRQ ở cycle cuối cùng của chính nó,
                // khiến lệnh kế tiếp luôn được chạy trước khi IRQ được xét.
                suppress_irq_check_this_instruction = true;
                FinishInstruction();
                return;
            }
        }
        else if (instr_cycle == 3)
        {
            read(pc); // dummy read thêm 1 cycle nếu crosspage
            FinishInstruction();
            return;
        }
        break;
    }

    // ================= JMP (indirect) =================
    case AddrClass::IND_:
    {
        if (instr_cycle == 1) { addr_lo = read(pc); pc++; }
        else if (instr_cycle == 2) { base_addr = read(pc); pc++; base_addr = (base_addr << 8) | addr_lo; }
        else if (instr_cycle == 3) { addr_lo = read(base_addr); }
        else if (instr_cycle == 4)
        {
            // Bug nổi tiếng của 6502 thật: không cộng qua trang khi đọc hi byte
            uint16_t hiAddr = (base_addr & 0xFF00) | ((base_addr + 1) & 0x00FF);
            uint16_t hi = read(hiAddr);
            pc = (hi << 8) | addr_lo;
            FinishInstruction();
            return;
        }
        break;
    }

    default:
        FinishInstruction();
        return;
        break;
    }

    instr_cycle++;
}

// ---- Bước 4: kết thúc lệnh — gọi operate() gốc rồi reset state machine ----
void CPU6502::FinishInstruction()
{
    auto& L2 = lookup2[opcode];
    auto& L = lookup[opcode];

    // Đồng bộ biến cũ mà operate() gốc đang dùng (addr_abs) với eff_addr mới tính được
    addr_abs = eff_addr;

    if (L2.operate && L2.otype != OpClass::BRANCH && L2.otype != OpClass::JUMP)
    {
        (this->*L2.operate)();
    }

    // CLI/SEI/PLP: hiệu lực IRQ-inhibition bị trì hoãn đúng 1 lệnh theo chuẩn 6502 thật.
    // KHÔNG sync irq_disable_shadow ngay - để giá trị CŨ được dùng cho đúng 1 lần xét
    // IRQ kế tiếp (owe_shadow_sync=true báo cho clock() biết cần đồng bộ sau khi xét xong).
    if (L.name == "CLI" || L.name == "SEI" || L.name == "PLP")
    {
        owe_shadow_sync = true;
    }
    else
    {
        irq_disable_shadow = GetFlag(I);
        owe_shadow_sync = false;
    }

    instr_cycle = 0;
    cycles = 0;
}
