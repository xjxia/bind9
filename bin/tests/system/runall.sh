#!/bin/sh
#
# Copyright (C) 2004, 2007, 2010-2012, 2014, 2015  Internet Systems Consortium, Inc. ("ISC")
# Copyright (C) 2000, 2001  Internet Software Consortium.
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
# REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
# AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
# INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
# LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
# OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
# PERFORMANCE OF THIS SOFTWARE.

#
# Run all the system tests.
#
# Note: Use "make check" to run all the system tests.  This script will just
# run those tests that require that each of their nameservers is the only one
# running on an IP address.
#

SYSTEMTESTTOP=.
. $SYSTEMTESTTOP/conf.sh

status=0

{
    for d in $SEQUENTIALDIRS
    do
            $SHELL run.sh "${@}" $d || status=1
    done
} 2>&1 | tee "systests.output"

$PERL testsock.pl || {
    cat <<EOF >&2
I:
I:NOTE: System tests were skipped because they require that the
I:      IP addresses 10.53.0.1 through 10.53.0.8 be configured
I:      as alias addresses on the loopback interface.  Please run
I:      "bin/tests/system/ifconfig.sh up" as root to configure them.
EOF
}

echo "I:System test result summary:"
grep '^R:' systests.output | sed -e 's/^/I: /' -e 's/R:[^:]*//' | sort | uniq -c
grep '^R:[^:]*:FAIL' systests.output > /dev/null && status=1

exit $status
