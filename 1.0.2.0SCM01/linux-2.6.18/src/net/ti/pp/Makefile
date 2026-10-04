################################################################################
#
# Copyright(c) 2009 Texas Instruments, Inc. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 2 of the License, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc., 59
# Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# The full GNU General Public License is included in this distribution in the
# file called LICENSE.
#
################################################################################

# PPM Deliverables.
obj-y := ti_ppm.o listlib.o ti_ppm_os.o hash.o ti_hil_core.o

# Sample Profiles.
obj-$(CONFIG_TI_HIL_PROFILE_INTRUSIVE) += hil_intrusive.o
obj-$(CONFIG_TI_HIL_PROFILE_STATIC)    += hil_static_profile.o

# Simulation Code (includes the firmware and PPD)
obj-$(CONFIG_TI_PACKET_PROCESSOR_SIMULATION) += pp.o ti_ppd.o

CFLAGS += -I$(TOPDIR)/include/asm-arm/arch/generic -I$(TOPDIR)/include/linux
