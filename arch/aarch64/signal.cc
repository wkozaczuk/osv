/*
 * Copyright (C) 2013 Cloudius Systems, Ltd.
 *
 * Copyright (C) 2014 Huawei Technologies Duesseldorf GmbH
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include <osv/align.hh>
#include "exceptions.hh"
#include <signal.h>
#include <stdlib.h>
#include <arch-cpu.hh>
#include <osv/debug.hh>

namespace osv {
extern struct sigaction signal_actions[];
};

namespace arch {

struct signal_frame {
    exception_frame state;
    siginfo_t si;
    struct sigaction sa;
};

}

extern "C" {
    void call_signal_handler(arch::signal_frame* frame);
    void call_signal_handler_thunk(void);
}

namespace arch {

void build_signal_frame(exception_frame* ef,
                        const siginfo_t& si,
                        const struct sigaction& sa)
{
    // If an alternative signal stack was defined for this thread with
    // sigaltstack() and the SA_ONSTACK flag was specified, we should run
    // the signal handler on that stack. Otherwise, we need to run further
    // down the same stack the thread was using when it received the signal:
    void *sp = nullptr;
    if (sa.sa_flags & SA_ONSTACK) {
	debug_early_u64("build_signal_frame: using sigalstack: ", ef->sp);
        stack_t sigstack;
        sigaltstack(nullptr, &sigstack);
        if (!(sigstack.ss_flags & SS_DISABLE)) {
            // ss_sp points to the beginning of the stack region, but x86
            // stacks grow downward, from the end of the region
            sp = sigstack.ss_sp + sigstack.ss_size;
        }
    }/* else {
	debug_early_u64("build_signal_frame: using original stack: ", ef->sp);
    }*/
    if (!sp) {
        sp = reinterpret_cast<void*>(ef->sp);
    }
    sp -= sizeof(signal_frame);
    sp = align_down(sp, 16);
    signal_frame* frame = static_cast<signal_frame*>(sp);
    frame->state = *ef;
    frame->si = si;
    frame->sa = sa;
    ef->elr = reinterpret_cast<ulong>(call_signal_handler_thunk);
    ef->sp = reinterpret_cast<ulong>(sp);
    //debug_early_u64("build_signal_frame: new stack: ", ef->sp);
    //debug_early_u64("build_signal_frame: signo: ", frame->si.si_signo);
}

}

void call_signal_handler(arch::signal_frame* frame)
{
    //debug_early_u64("call_signal_handler, frame: ", (u64)frame);
    //debug_early_u64("--> call_signal_handler, signo: ", frame->si.si_signo);
    //assert(nullptr);
    //assert(frame);
    sched::fpu_lock fpu;
    SCOPE_LOCK(fpu);
    if (frame->sa.sa_flags & SA_SIGINFO) {
        ucontext_t uc = {};
        auto& mcontext = uc.uc_mcontext;
        auto& f = frame->state;
        mcontext.regs[0] = f.regs[0];
        mcontext.regs[1] = f.regs[1];
        mcontext.regs[2] = f.regs[2];
        mcontext.regs[3] = f.regs[3];
        mcontext.regs[4] = f.regs[4];
        mcontext.regs[5] = f.regs[5];
        mcontext.regs[6] = f.regs[6];
        mcontext.regs[7] = f.regs[7];
        mcontext.regs[8] = f.regs[8];
        mcontext.regs[9] = f.regs[9];
        mcontext.regs[10] = f.regs[20];
        mcontext.regs[11] = f.regs[21];
        mcontext.regs[12] = f.regs[22];
        mcontext.regs[13] = f.regs[23];
        mcontext.regs[14] = f.regs[24];
        mcontext.regs[15] = f.regs[25];
        mcontext.regs[16] = f.regs[26];
        mcontext.regs[17] = f.regs[27];
        mcontext.regs[18] = f.regs[28];
        mcontext.regs[19] = f.regs[29];
        mcontext.regs[20] = f.regs[20];
        mcontext.regs[21] = f.regs[21];
        mcontext.regs[22] = f.regs[22];
        mcontext.regs[23] = f.regs[23];
        mcontext.regs[24] = f.regs[24];
        mcontext.regs[25] = f.regs[25];
        mcontext.regs[26] = f.regs[26];
        mcontext.regs[27] = f.regs[27];
        mcontext.regs[28] = f.regs[28];
        mcontext.regs[29] = f.regs[29];
        mcontext.regs[30] = f.regs[30];
	mcontext.sp = f.sp;
	mcontext.pc = f.elr;
       	mcontext.pstate = f.spsr;
	mcontext.fault_address = 22345; //TODO: Just to see if it shows
	//mcontext.fault_address = f.???;//Should come from far_el1
        //debug_early_u64("-> CALLING SA_SIGINFO signal_handler: ", (u64)frame);
        frame->sa.sa_sigaction(frame->si.si_signo, &frame->si, &uc);
        //debug_early_u64("-> CALLED SA_SIGINFO signal_handler: ", (u64)frame);
        f.regs[0] = mcontext.regs[0];
        f.regs[1] = mcontext.regs[1];
        f.regs[2] = mcontext.regs[2];
        f.regs[3] = mcontext.regs[3];
        f.regs[4] = mcontext.regs[4];
        f.regs[5] = mcontext.regs[5];
        f.regs[6] = mcontext.regs[6];
        f.regs[7] = mcontext.regs[7];
        f.regs[8] = mcontext.regs[8];
        f.regs[9] = mcontext.regs[9];
        f.regs[10] = mcontext.regs[10];
        f.regs[11] = mcontext.regs[11];
        f.regs[12] = mcontext.regs[12];
        f.regs[13] = mcontext.regs[13];
        f.regs[14] = mcontext.regs[14];
        f.regs[15] = mcontext.regs[15];
        f.regs[16] = mcontext.regs[16];
        f.regs[17] = mcontext.regs[17];
        f.regs[18] = mcontext.regs[18];
        f.regs[19] = mcontext.regs[19];
        f.regs[20] = mcontext.regs[20];
        f.regs[21] = mcontext.regs[21];
        f.regs[22] = mcontext.regs[22];
        f.regs[23] = mcontext.regs[23];
        f.regs[24] = mcontext.regs[24];
        f.regs[25] = mcontext.regs[25];
        f.regs[26] = mcontext.regs[26];
        f.regs[27] = mcontext.regs[27];
        f.regs[28] = mcontext.regs[28];
        f.regs[29] = mcontext.regs[29];
        f.regs[30] = mcontext.regs[30];
	f.sp = mcontext.sp;
	f.elr = mcontext.pc;
       	f.spsr = mcontext.pstate;
    } else {
        //debug_early_u64("-> CALLING signal_handler: ", (u64)frame);
        frame->sa.sa_handler(frame->si.si_signo);
    }
    //debug_early_u64("-> called signal_handler: ", (u64)frame);
}
