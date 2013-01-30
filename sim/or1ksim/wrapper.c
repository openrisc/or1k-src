/* GDB Simulator wrapper for Or1ksim, the OpenRISC architectural simulator

   Copyright 1988-2008, Free Software Foundation, Inc.
   Copyright (C) 2010 Embecosm Limited

   Contributor Jeremy Bennett <jeremy.bennett@embecosm.com>

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 3 of the License, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
   more details.

   You should have received a copy of the GNU General Public License along
   with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/*---------------------------------------------------------------------------*/
/* This is a wrapper for Or1ksim, suitable for use as a GDB simulator.

   The code tries to follow the GDB coding style.

   Commenting is Doxygen compatible.                                         */
/*---------------------------------------------------------------------------*/

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "ansidecl.h"
#include "config.h"
#include "gdb/callback.h"
#include "gdb/remote-sim.h"
#include "sim-utils.h"
#include "targ-vals.h"

#include "or1ksim.h"
#include "or32sim.h"

/* Define this to turn on debug messages */
/* #define OR32_SIM_DEBUG */

/* ------------------------------------------------------------------------- */
/*!Create a fully initialized simulator instance.

   This function is called when the simulator is selected from the gdb command
   line.

   While the simulator configuration can be parameterized by (in decreasing
   precedence) argv's SIM-OPTION, argv's TARGET-PROGRAM and the abfd argument,
   the successful creation of the simulator shall not dependent on the
   presence of any of these arguments/options.

   For a hardware simulator the created simulator shall be sufficiently
   initialized to handle, without restrictions any client requests (including
   memory reads/writes, register fetch/stores and a resume).

   For a process simulator, the process is not created until a call to
   sim_create_inferior.

   We do the following on a first call.
   - parse the options
   - 
   @todo Eventually we should use the option parser built into the GDB
         simulator (see common/sim-options.h). However since this is minimally
         documented, and we have only the one option, for now we do it
         ourselves.

   @note We seem to capable of being called twice. We use the static
         "global_sd" variable to keep track of this. Second and subsequent
         calls do nothing, but return the previously opened simulator
         description. 

   @param[in] kind  Specifies how the simulator shall be used.  Currently
                    there are only two kinds: stand-alone and debug.

   @param[in] callback  Specifies a standard host callback (defined in
                        callback.h).

   @param[in] abfd      When non NULL, designates a target program.  The
                        program is not loaded.

   @param[in] argv      A standard ARGV pointer such as that passed from the
                        command line.  The syntax of the argument list is is
                        assumed to be ``SIM-PROG { SIM-OPTION } [
                        TARGET-PROGRAM { TARGET-OPTION } ]''.

			The trailing TARGET-PROGRAM and args are only valid
                        for a stand-alone simulator.

			The argument list is null terminated!

   @return On success, the result is a non NULL descriptor that shall be
           passed to the other sim_foo functions.                            */
/* ------------------------------------------------------------------------- */
SIM_DESC
sim_open (SIM_OPEN_KIND                kind,
	  struct host_callback_struct *callback,
	  struct bfd                  *abfd,
	  char                        *argv[])
{
  /*!A global record of the simulator description */
  static SIM_DESC  static_sd = NULL;

#ifdef OR32_SIM_DEBUG
  printf ("sim_open called\n", (int) kind);
#endif

  /* If static_sd is not yet allocated, we allocate it and mark the simulator
     as not yet open. This is the only time we can process any custom
     arguments and only time we initialize the simulator. */
  if (NULL == static_sd)
    {
      int    local_argc;		/* Our local argv with extra args */
      char **local_argv;

      int    argc;			/* How many args originally */
      int    i, j;			/* For local argv */
      int    mem_defined_p = 0;		/* Have we requested a memory size? */
      int    sysroot_defined_p = 0;	/* Have we requested --sysroot ?*/

      int    res;			/* Result of initialization */

      static_sd = (SIM_DESC) malloc (sizeof (*static_sd));
      static_sd->sim_open = 0;

      /* Count the number of arguments and see if we have specified either a
	 config file or a memory size. */
      for (argc = 1; NULL != argv[argc]; argc++)
	{
#ifdef OR32_SIM_DEBUG
	  printf ("argv[%d] = %s\n", argc, argv[argc]);
#endif
	  if ((0 == strcmp (argv[argc], "-f"))    ||
	      (0 == strcmp (argv[argc], "-file")) ||
	      (0 == strcmp (argv[argc], "-m"))    ||
	      (0 == strcmp (argv[argc], "-memory")))
	    {
	      mem_defined_p = 1;
	    }

	  if (0 == strncmp (argv[argc], "--sysroot", 9))
	      sysroot_defined_p = argc;
	}

      /* If we have no memory defined, we give it a default 8MB. We also always
	 run quiet. So we must define our own argument vector */
      local_argc = mem_defined_p ? argc + 1 : argc + 3;

      /* We don't support --sysroot argument, so if we asked, remove it from
	the list. */
      local_argc = sysroot_defined_p ? local_argc - 1 : local_argc;

      local_argv = malloc ((local_argc + 1) * sizeof (char *));

      j = 0;
      for (i = 0 ; i < argc; i++)
	{
	    if (i != sysroot_defined_p)
	      local_argv[j++] = argv[i];
	}

      local_argv[j++] = "--quiet";

      if (!mem_defined_p)
	{
	  local_argv[j++] = "--memory";
	  local_argv[j++] = "8M";
	}

      local_argv[j] = NULL;

      /* Try to initialize, then we can free the local argument vector. If we
	 fail to initialize return NULL to indicate that failure. */ 
      res = or1ksim_init (local_argc, local_argv, NULL, NULL, NULL);
      free (local_argv);

      if (res)
	{
	  return  NULL;			/* Failure */
	}
    }

  /* We have either initialized a new simulator, or already have an intialized
     simulator. Populate the descriptor and stall the processor, the return
     the descriptor. */
  static_sd->callback    = callback;
  static_sd->is_debug    = (kind == SIM_OPEN_DEBUG);
  static_sd->myname      = (char *)xstrdup (argv[0]);
  static_sd->sim_open    = 1;
  static_sd->last_reason = sim_running;
  static_sd->last_rc     = TARGET_SIGNAL_NONE;
  static_sd->entry_point = OR32_RESET_EXCEPTION;
  static_sd->resume_npc  = OR32_RESET_EXCEPTION;

  or1ksim_set_stall_state (0);

  return  static_sd;

}	/* sim_open () */


/* ------------------------------------------------------------------------- */
/*!Destroy a simulator instance.

   We never actually close the simulator, because we have no way to
   reinitialize it. Instead we just stall the processor and mark it closed.

   @param[in] sd        Simulation descriptor from sim_open ().
   @param[in] quitting  Non-zero if we cannot hang on errors.                */
/* ------------------------------------------------------------------------- */
void
sim_close (SIM_DESC  sd,
	   int       quitting)
{
#ifdef OR32_SIM_DEBUG
  printf ("sim_close called\n");
#endif

  if (NULL == sd)
    {
      fprintf (stderr,
	       "Warning: Attempt to close non-open simulation: ignored.\n");
    }
  else
    {
      free (sd->myname);
      sd->sim_open = 0;
      or1ksim_set_stall_state (0);
    }
}	/* sim_close () */


/* ------------------------------------------------------------------------- */
/*!Load program PROG into the simulators memory.

   Hardware simulator: Normally, each program section is written into
   memory according to that sections LMA using physical (direct)
   addressing.  The exception being systems, such as PPC/CHRP, which
   support more complicated program loaders.  A call to this function
   should not effect the state of the processor registers.  Multiple
   calls to this function are permitted and have an accumulative
   effect.

   Process simulator: Calls to this function may be ignored.

   @todo Most hardware simulators load the image at the VMA using
         virtual addressing.

   @todo For some hardware targets, before a loaded program can be executed,
         it requires the manipulation of VM registers and tables.  Such
         manipulation should probably (?) occure in sim_create_inferior ().

   @param[in] sd        Simulation descriptor from sim_open ().
   @param[in] prog      The name of the program
   @param[in] abfd      If non-NULL, the BFD for the file has already been
                        opened. 
   @param[in] from_tty  Not sure what this does. Probably indicates this is a
                        command line load? Anyway we don't use it.

   @return  A return code indicating success.                                */
/* ------------------------------------------------------------------------- */
SIM_RC
sim_load (SIM_DESC    sd,
	  char       *prog,
	  struct bfd *abfd,
	  int         from_tty)
{
  bfd *prog_bfd;

#ifdef OR32_SIM_DEBUG
  printf ("sim_load called\n");
#endif

  /* Use the built in loader, which will in turn use our write function. */
  prog_bfd = sim_load_file (sd, sd->myname, sd->callback, prog, abfd,
			    sd->is_debug, 0, sim_write);

  if (NULL == prog_bfd)
    {
      return SIM_RC_FAIL;
    }

  /* If the BFD was not already open, then close the loaded program. */
  if (NULL == abfd)
    {
      bfd_close (prog_bfd);
    }

  return  SIM_RC_OK;

}	/* sim_load () */


/* ------------------------------------------------------------------------- */
/*!Prepare to run the simulated program.

   Hardware simulator: This function shall initialize the processor
   registers to a known value.  The program counter and possibly stack
   pointer shall be set using information obtained from ABFD (or
   hardware reset defaults).  ARGV and ENV, dependant on the target
   ABI, may be written to memory.

   Process simulator: After a call to this function, a new process
   instance shall exist. The TEXT, DATA, BSS and stack regions shall
   all be initialized, ARGV and ENV shall be written to process
   address space (according to the applicable ABI) and the program
   counter and stack pointer set accordingly.

   ABFD, if not NULL, provides initial processor state information.
   ARGV and ENV, if non NULL, are NULL terminated lists of pointers.

   We perform the following steps:
   - stall the processor
   - set the entry point to the entry point in the BFD, or the reset
     vector if the BFD is not available.
   - set the resumption NPC to the reset vector. We always do this, to ensure
     the library is initialized.

   @param[in] sd    Simulation descriptor from sim_open ().
   @param[in] abfd  If non-NULL provides initial processor state information.
   @param[in] argv  Vector of arguments to the program. We don't use this
   @param[in] env   Vector of environment data. We don't use this.

   @return  A return code indicating success.                                */
/* ------------------------------------------------------------------------- */
SIM_RC
sim_create_inferior (SIM_DESC     sd,
		     struct bfd  *abfd,
		     char       **argv  ATTRIBUTE_UNUSED,
		     char       **env   ATTRIBUTE_UNUSED)
{
#ifdef OR32_SIM_DEBUG
  printf ("sim_create_inferior called\n");
#endif

  or1ksim_set_stall_state (1);
  sd->entry_point = (NULL == abfd) ? OR32_RESET_EXCEPTION :
                                    bfd_get_start_address (abfd);
  sd->resume_npc  = OR32_RESET_EXCEPTION;

  return  SIM_RC_OK;

}	/* sim_create_inferior () */


/* ------------------------------------------------------------------------- */
/*!Fetch bytes from the simulated program's memory.

   @param[in]  sd   Simulation descriptor from sim_open (). We don't use
                    this.
   @param[in]  mem  The address in memory to fetch from.
   @param[out] buf  Where to put the read data
   @param[in]  len  Number of bytes to fetch

   @return  Number of bytes read, or zero if error.                          */
/* ------------------------------------------------------------------------- */
int
sim_read (SIM_DESC       sd  ATTRIBUTE_UNUSED,
	  SIM_ADDR       mem,
	  unsigned char *buf,
	  int            len)
{
  int res = or1ksim_read_mem (mem, buf, len);

#ifdef OR32_SIM_DEBUG
  printf ("Reading %d bytes from %08p\n", len, (void *) mem);
#endif

  return  res;

}      /* sim_read () */


/* ------------------------------------------------------------------------- */
/*!Store bytes to the simulated program's memory.

   @param[in] sd   Simulation descriptor from sim_open (). We don't use
                   this.
   @param[in] mem  The address in memory to write to.
   @param[in] buf  The data to write
   @param[in] len  Number of bytes to write

   @return  Number of byte written, or zero if error.                        */
/* ------------------------------------------------------------------------- */
int
sim_write (SIM_DESC             sd  ATTRIBUTE_UNUSED,
	   SIM_ADDR             mem,
	   const unsigned char *buf,
	   int                  len)
{
#ifdef OR32_SIM_DEBUG
  printf ("Writing %d bytes to %08p\n", len, (void *) mem);
#endif

  return  or1ksim_write_mem ((unsigned int) mem, buf, len);

}	/* sim_write () */


/* ------------------------------------------------------------------------- */
/*!Fetch a register from the simulation

   We get the register back as a 32-bit value. However we must convert it to a
   character array <em>in target endian order</em>.

   The exception is if the register is the NPC, which is only written just
   before resumption, to avoid pipeline confusion. It is fetched from the SD.

   @param[in]  sd     Simulation descriptor from sim_open (). We don't use
                      this.
   @param[in]  regno  The register to fetch
   @param[out] buf    Buffer of length bytes to store the result. Data is
                      only transferred if length matches the register length
                      (the actual register size is still returned).
   @param[in]  len    Size of buf, which should match the size of the
                      register.

   @return  The actual size of the register, or zero if regno is not
            applicable. Legacy implementations return -1.
/* ------------------------------------------------------------------------- */
int
sim_fetch_register (SIM_DESC       sd,
		    int            regno,
		    unsigned char *buf,
		    int            len)
{
  unsigned long int  regval;
  int                res;

#ifdef OR32_SIM_DEBUG
  printf ("sim_fetch_register (regno=%d) called\n", regno);
#endif
  if (4 != len)
    {
      fprintf (stderr, "Invalid register length %d\n", len);
      return  0;
    }

  if (OR32_NPC_REGNUM == regno)
    {
      regval = sd->resume_npc;
      res    = 4;
    }
  else
    {
      int res = or1ksim_read_reg (regno, &regval) ? 4 : 0;
    }

  /* Convert to target (big) endian */
  if (res)
    {
      buf[0] = (regval >> 24) & 0xff;
      buf[1] = (regval >> 16) & 0xff;
      buf[2] = (regval >>  8) & 0xff;
      buf[3] =  regval        & 0xff;

      return  4;			/* Success */
    }
  else
    {
      return  0;			/* Failure */
    }
}	/* sim_fetch_register () */


/* ------------------------------------------------------------------------- */
/*!Store a register to the simulation

   We write the register back as a 32-bit value. However we must convert it from
   a character array <em>in target endian order</em>.

   The exception is if the register is the NPC, which is only written just
   before resumption, to avoid pipeline confusion. It is saved in the SD.

   @param[in] sd     Simulation descriptor from sim_open (). We don't use
                     this.
   @param[in] regno  The register to store
   @param[in] buf    Buffer of length bytes with the data to store. Data is
                     only transferred if length matches the register length
                     (the actual register size is still returned).
   @param[in] len    Size of buf, which should match the size of the
                     register.

   @return  The actual size of the register, or zero if regno is not
            applicable. Legacy implementations return -1.
/* ------------------------------------------------------------------------- */
int
sim_store_register (SIM_DESC       sd,
		    int            regno,
		    unsigned char *buf,
		    int            len)
{
  unsigned int  regval;

#ifdef OR32_SIM_DEBUG
  printf ("sim_store_register (regno=%d\n) called\n", regno);
#endif

  if (4 != len)
    {
      fprintf (stderr, "Invalid register length %d\n", len);
      return  0;
    }

  /* Convert from target (big) endian */
  regval = (((unsigned int) buf[0]) << 24) |
           (((unsigned int) buf[1]) << 16) |
           (((unsigned int) buf[2]) <<  8) |
           (((unsigned int) buf[3])      );

#ifdef OR32_SIM_DEBUG
  printf ("Writing register 0x%02x, value 0x%08x\n", regno, regval);
#endif

  if (OR32_NPC_REGNUM == regno)
    {
      sd->resume_npc = regval;
      return  4;			/* Reg length in bytes */
    }
  else
    {
      return  or1ksim_write_reg (regno, regval) ? 4 : 0;
    }
}	/* sim_store_register () */


/* ------------------------------------------------------------------------- */
/* Print whatever statistics the simulator has collected.

   @param[in] sd       Simulation descriptor from sim_open (). We don't use
                       this.
   @param[in] verbose  Currently unused, and should always be zero.          */
/* ------------------------------------------------------------------------- */
void
sim_info (SIM_DESC  sd       ATTRIBUTE_UNUSED,
	  int       verbose  ATTRIBUTE_UNUSED)
{
}	/* sim_info () */


/* ------------------------------------------------------------------------- */
/*!Run (or resume) the simulated program.

   Hardware simulator: If the SIGRC value returned by
   sim_stop_reason() is passed back to the simulator via siggnal then
   the hardware simulator shall correctly deliver the hardware event
   indicated by that signal.  If a value of zero is passed in then the
   simulation will continue as if there were no outstanding signal.
   The effect of any other siggnal value is is implementation
   dependant.

   Process simulator: If SIGRC is non-zero then the corresponding
   signal is delivered to the simulated program and execution is then
   continued.  A zero SIGRC value indicates that the program should
   continue as normal.

   We carry out the following
   - Clear the debug reason register
   - Clear watchpoing break generation in debug mode register 2
   - Set the debug unit to handle TRAP exceptions
   - If stepping, set the single step trigger in debug mode register 1
   - Write the resume_npc if it differs from the actual NPC.
   - Unstall the processor
   - Run the processor.

   On execution completion, we determine the reason for the halt. If it is a
   breakpoint, we mark the resumption NPC to be the PPC (so we redo the NPC
   location).

   @param[in] sd       Simulation descriptor from sim_open ().
   @param[in] step     When non-zero indicates that only a single simulator
                       cycle should be emulated.
   @param[in] siggnal  If non-zero is a (HOST) SIGRC value indicating the type
                       of event (hardware interrupt, signal) to be delivered
                       to the simulated program.                             */
/* ------------------------------------------------------------------------- */
void
sim_resume (SIM_DESC  sd,
	    int       step,
	    int       siggnal)
{
  unsigned long int  npc;		/* Next Program Counter */
  unsigned long int  drr;		/* Debug Reason Register */
  unsigned long int  dsr;		/* Debug Stop Register */
  unsigned long int  dmr1;		/* Debug Mode Register 1*/
  unsigned long int  dmr2;		/* Debug Mode Register 2*/

  unsigned long int  retval;		/* Return value on Or1ksim exit */

  unsigned long int  cycles;            /* Length of run in cycles */
  int                res;		/* Result of a run. */

#ifdef OR32_SIM_DEBUG
  printf ("sim_resume called\n");
#endif

  /* Clear Debug Reason Register and watchpoint break generation in Debug Mode
     Register 2 */
  (void) or1ksim_write_spr (OR32_SPR_DRR, 0);

  (void) or1ksim_read_spr (OR32_SPR_DMR2, &dmr2);
  dmr2 &= ~OR32_SPR_DMR2_WGB;
  (void) or1ksim_write_spr (OR32_SPR_DMR2, dmr2);

  /* Set debug unit to handle TRAP exceptions */
  (void) or1ksim_read_spr (OR32_SPR_DSR, &dsr);
  dsr |= OR32_SPR_DSR_TE;
  (void) or1ksim_write_spr (OR32_SPR_DSR, dsr);

  /* Set the single step trigger in Debug Mode Register 1 if we are
     stepping. Otherwise clear it! */
  if (step)
    {
      (void) or1ksim_read_spr (OR32_SPR_DMR1, &dmr1);
      dmr1 |= OR32_SPR_DMR1_ST;
      (void) or1ksim_write_spr (OR32_SPR_DMR1, dmr1);
    }
  else
    {
      (void) or1ksim_read_spr (OR32_SPR_DMR1, &dmr1);
      dmr1 &= ~OR32_SPR_DMR1_ST;
      (void) or1ksim_write_spr (OR32_SPR_DMR1, dmr1);
    }

  /* Set the NPC if it has changed */
  (void) or1ksim_read_reg (OR32_NPC_REGNUM, &npc);

#ifdef OR32_SIM_DEBUG
  printf ("  npc = 0x%08lx, resume_npc = 0x%08lx\n", npc, sd->resume_npc);
#endif

  if (npc != sd->resume_npc)
    {
      (void) or1ksim_write_reg (OR32_NPC_REGNUM, sd->resume_npc);
    }

  /* Set a time point */
  or1ksim_set_time_point ();

  /* Unstall and run */
  or1ksim_set_stall_state (0);
  res = or1ksim_run (-1.0);

  /* Determine the reason for stopping. If we hit a breakpoint, then the
     resumption NPC must be set to the PPC to allow re-execution of the
     trapped instruction. */
  switch (res)
    {
    case OR1KSIM_RC_HALTED:

#ifdef OR32_SIM_DEBUG
      (void) or1ksim_read_reg (OR32_NPC_REGNUM, &npc);
      printf ("  execution halted at 0x%08lx.\n", npc);
#endif

      sd->last_reason = sim_exited;
      (void) or1ksim_read_reg (OR32_FIRST_ARG_REGNUM, &retval);
      sd->last_rc     = (unsigned int) retval;
      sd->resume_npc  = OR32_RESET_EXCEPTION;
      cycles = (long int) (or1ksim_get_time_period ()
			   * (double) or1ksim_clock_rate());
      break;

    case OR1KSIM_RC_BRKPT:

#ifdef OR32_SIM_DEBUG
      printf ("  execution hit breakpoint.\n");
#endif

      sd->last_reason = sim_stopped;
      sd->last_rc     = TARGET_SIGNAL_TRAP;

      /* This could have been a breakpoint or single step. */
      if (step)
	{
	  (void) or1ksim_read_reg (OR32_NPC_REGNUM, &(sd->resume_npc));
	}
      else
	{
	  (void) or1ksim_read_reg (OR32_PPC_REGNUM, &(sd->resume_npc));
	}

      break;

    case OR1KSIM_RC_OK:
      /* Should not happen */
      fprintf (stderr, "Ooops. Didn't expect OK return from Or1ksim.\n");

      sd->last_reason = sim_running;		/* Should trigger an error! */
      sd->last_rc     = TARGET_SIGNAL_NONE;
      (void) or1ksim_read_reg (OR32_NPC_REGNUM, &(sd->resume_npc));
      break;
    }
}	/* sim_resume () */


/* ------------------------------------------------------------------------- */
/*!Asynchronous request to stop the simulation.

   @param[in] sd  Simulation descriptor from sim_open (). We don't use this.

   @return  Non-zero indicates that the simulator is able to handle the
            request.                                                         */ 
/* ------------------------------------------------------------------------- */
int sim_stop (SIM_DESC  sd  ATTRIBUTE_UNUSED)
{
#ifdef OR32_SIM_DEBUG
  printf ("sim_stop called\n");
#endif

  return  0;			/* We don't support this */

}	/* sim_stop () */


/* ------------------------------------------------------------------------- */
/*!Fetch the REASON why the program stopped.

   The reason enumeration indicates why:

   - sim_exited:    The program has terminated. sigrc indicates the target
                    dependant exit status.

   - sim_stopped:   The program has stopped.  sigrc uses the host's signal
                    numbering as a way of identifying the reaon: program
                    interrupted by user via a sim_stop request (SIGINT); a
                    breakpoint instruction (SIGTRAP); a completed single step
                    (SIGTRAP); an internal error condition (SIGABRT); an
                    illegal instruction (SIGILL); Access to an undefined
                    memory region (SIGSEGV); Mis-aligned memory access
                    (SIGBUS).

		    For some signals information in addition to the signal
                    number may be retained by the simulator (e.g. offending
                    address), that information is not directly accessable via
                    this interface.

   - sim_signalled: The program has been terminated by a signal. The
                    simulator has encountered target code that causes the the
                    program to exit with signal sigrc.

   - sim_running:
   - sim_polling:   The return of one of these values indicates a problem
                    internal to the simulator.

   @param[in]  sd      Simulation descriptor from sim_open ().
   @param[out] reason  The reason for stopping
   @param[out] sigrc   Supplementary information for some values of reason.  */
/* ------------------------------------------------------------------------- */
void
sim_stop_reason (SIM_DESC       sd,
		 enum sim_stop *reason,
		 int           *sigrc)
 {
   *reason = sd->last_reason;
   *sigrc  = sd->last_rc;

}	/* sim_stop_reason () */


/* ------------------------------------------------------------------------- */
/* Passthru for other commands that the simulator might support.

   Simulators should be prepared to deal with any combination of NULL
   or empty command.

   This implementation currently does nothing.

   @param[in] sd  Simulation descriptor from sim_open (). We don't use this.
   @param[in] cmd The command to pass through.                               */
/* ------------------------------------------------------------------------- */
void
sim_do_command (SIM_DESC  sd   ATTRIBUTE_UNUSED,
		char     *cmd  ATTRIBUTE_UNUSED)
{
#ifdef OR32_SIM_DEBUG
  printf ("sim_do_command called\n");
#endif

}	/* sim_do_command () */


/* ------------------------------------------------------------------------- */
/* Set the default host_callback_struct

   @note Deprecated, but implemented, since it is still required for linking.

   This implementation currently does nothing.

   @param[in] ptr  The host_callback_struct pointer. Unused here.            */
/* ------------------------------------------------------------------------- */
void
sim_set_callbacks (struct host_callback_struct *ptr ATTRIBUTE_UNUSED)
{
#ifdef OR32_SIM_DEBUG
  printf ("sim_set_callbacks called\n");
#endif

}	/* sim_set_callbacks () */


/* ------------------------------------------------------------------------- */
/* Set the size of the simulator memory array.

   @note Deprecated, but implemented, since it is still required for linking.

   This implementation currently does nothing.

   @param[in] size  The memory size to use. Unused here.                     */
/* ------------------------------------------------------------------------- */
void
sim_size (int  size  ATTRIBUTE_UNUSED)
{
#ifdef OR32_SIM_DEBUG
  printf ("sim_size called\n");
#endif

}	/* sim_size () */


/* ------------------------------------------------------------------------- */
/* Single step the simulator with tracing enabled.

   @note Deprecated, but implemented, since it is still required for linking.

   This implementation currently does nothing.

   @param[in] sd  The simulator description struct. Unused here.             */
/* ------------------------------------------------------------------------- */
void
sim_trace (SIM_DESC  sd  ATTRIBUTE_UNUSED)
{
#ifdef OR32_SIM_DEBUG
  printf ("sim_trace called\n");
#endif

}	/* sim_trace () */

char **
sim_complete_command (SIM_DESC sd, char *text, char *word)
{
  return NULL;
}