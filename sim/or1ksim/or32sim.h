/* Header for GDB Simulator wrapper for Or1ksim

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

/* GDB signal numbers */
#define TARGET_SIGNAL_NONE  0		/*!< No signal */
#define TARGET_SIGNAL_TRAP  5		/*!< Breakpoint hit */

/*! Number of registers */
#define  OR32_MAX_GPRS  32

/* Particular registers */
#define OR32_FIRST_ARG_REGNUM  3			/*!< First arg reg */
#define OR32_PPC_REGNUM        (OR32_MAX_GPRS + 0)	/*!< Previous PC */
#define OR32_NPC_REGNUM        (OR32_MAX_GPRS + 1)	/*!< Next PC */
#define OR32_SR_REGNUM         (OR32_MAX_GPRS + 2)	/*!< Supervision Reg */

/* Debug SPRs */
#define OR32_SPR_NPC    0x0010		/*!< Next Program Counter */
#define OR32_SPR_SR     0x0011		/*!< Supervision Register */
#define OR32_SPR_PPC    0x0012		/*!< Previous Program Counter */

#define OR32_SPR_DMR1	0x3010		/*!< Debug Mode Register 1 */
#define OR32_SPR_DMR2	0x3011		/*!< Debug Mode Register 2 */
#define OR32_SPR_DSR    0x3014		/*!< Debug Stop Register */
#define OR32_SPR_DRR	0x3015		/*!< Debug Reason Register */

/* Debug SPR bit fields */
#define OR32_SPR_DMR1_ST   0x00400000	/*!< Single-step trace*/
#define OR32_SPR_DMR2_WGB  0x003ff000	/*!< Watchpoints which breakpoint */
#define OR32_SPR_DSR_TE	   0x00002000	/*!< Trap exception bit */

/* OR1K exception vector addresses */
#define OR32_RESET_EXCEPTION  0x100	/*!< Reset exception vector */

/* ------------------------------------------------------------------------- */
/*!A structure to hold the state of a simulation instance.

   This is the typedef SIM_DESC.

   The entries are
   - A flag which is true if we are used for debug rather than standalone (i.e
     we were opened with type SIM_OPEN_DEBUG)
   - The callback function supplied to the sim_open () function
   - This simulator's name (argv[0] supplied to sim_open ()
   - A flag to indicate the simulator has been opened.
   - The last reason the simulator stopped
   - The signal associated with the last stop, or the exit code from the last
     exit.
   - The entry point to the program if available, otherwise the reset
     exception vector address.
   - The NPC with which to resume. So as not to destroy the pipeline, this is
     only written immediately before unstalling.                             */
/* ------------------------------------------------------------------------- */
struct sim_state
{
  int                          is_debug;
  struct host_callback_struct *callback;
  char                        *myname;
  int                          sim_open;
  enum sim_stop                last_reason;
  unsigned int		       last_rc;
  unsigned long int            entry_point;
  unsigned long int            resume_npc;
  
};


