
# -*- sh -*-

#  Copyright (c) Abraham vd Merwe <abz@blio.com>
#  All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions
#  are met:
#  1. Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#
#  2. Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#  3. Neither the name of the author nor the names of other contributors
#     may be used to endorse or promote products derived from this software
#     without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
#  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#  ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
#  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
#  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
#  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
#  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
#  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# path to toplevel directory from here
TOPDIR = ..

# subdirectories (leave as is if there is no subdirectories)
DIR =

# names of object files
OBJ =

# program name (leave as is if there is no program)
PRG =

# library name (leave as is if there is no library)
LIB =

include $(TOPDIR)/paths.mk
include $(TOPDIR)/defs.mk
include $(TOPDIR)/vars.mk
include $(TOPDIR)/rules.mk

MAN = ber_decode_null.3ber					\
	ber_encode_null.3ber					\
	makeoid.3ber

DECODE_LNK = ber_decode_sequence.3ber.gz	\
	ber_decode_get_request.3ber.gz			\
	ber_decode_get_next_request.3ber.gz		\
	ber_decode_get_response.3ber.gz			\
	ber_decode_set_request.3ber.gz			\
	ber_decode_trap.3ber.gz					\
	ber_decode_get_bulk_request.3ber.gz		\
	ber_decode_inform_request.3ber.gz		\
	ber_decode_snmpv2_trap.3ber.gz			\
	ber_decode_counter64.3ber.gz			\
	ber_decode_integer.3ber.gz				\
	ber_decode_counter32.3ber.gz			\
	ber_decode_gauge32.3ber.gz				\
	ber_decode_timeticks.3ber.gz			\
	ber_decode_ipaddress.3ber.gz			\
	ber_decode_octet_string.3ber.gz			\
	ber_decode_string.3ber.gz				\
	ber_decode_oid.3ber.gz					\
	ber_decode_get_message.3ber.gz			\
	ber_decode_put_message.3ber.gz			\
	ber_decode_auth.3ber.gz					\
	ber_decode_reset.3ber.gz				\
	ber_decode_set_level.3ber.gz

ENCODE_LNK = ber_encode_sequence.3ber.gz	\
	ber_encode_get_request.3ber.gz			\
	ber_encode_get_next_request.3ber.gz		\
	ber_encode_get_response.3ber.gz			\
	ber_encode_set_request.3ber.gz			\
	ber_encode_trap.3ber.gz					\
	ber_encode_get_bulk_request.3ber.gz		\
	ber_encode_inform_request.3ber.gz		\
	ber_encode_snmpv2_trap.3ber.gz			\
	ber_encode_counter64.3ber.gz			\
	ber_encode_integer.3ber.gz				\
	ber_encode_counter32.3ber.gz			\
	ber_encode_gauge32.3ber.gz				\
	ber_encode_timeticks.3ber.gz			\
	ber_encode_ipaddress.3ber.gz			\
	ber_encode_octet_string.3ber.gz			\
	ber_encode_string.3ber.gz				\
	ber_encode_oid.3ber.gz					\
	ber_encode_get_message.3ber.gz			\
	ber_encode_put_message.3ber.gz			\
	ber_encode_auth.3ber.gz					\
	ber_encode_reset.3ber.gz				\
	ber_encode_set_level.3ber.gz

MAN_LNK = $(DECODE_LNK) $(ENCODE_LNK)

install::
	$(INSTALL) -d $(mandir)/man3
	set -e; for F in $(MAN); do $(INSTALL) -c -m 0644 $$F $(mandir)/man3 && gzip -9 $(mandir)/man3/$$F; done
	set -e; for F in $(DECODE_LNK); do ln -sf ber_decode_null.3ber.gz $(mandir)/man3/$$F; done
	set -e; for F in $(ENCODE_LNK); do ln -sf ber_encode_null.3ber.gz $(mandir)/man3/$$F; done

uninstall::
	set -e; for F in $(MAN_LNK) $(MAN); do rm -f $(mandir)/man3/$$F; done

