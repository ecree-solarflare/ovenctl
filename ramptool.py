#! /usr/bin/env python
# encoding: utf-8
#
# Copyright Solarflare Communications Inc., 2012-13
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of Solarflare Communications Inc. nor the names of its
#       contributors may be used to endorse or promote products derived from
#       this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL SOLARFLARE COMMUNICATIONS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import optparse, sys, math

AMBIENT = 25

def parse_cmdline():
    parser = optparse.OptionParser()
    parser.usage = u"""%prog -T temp -H hold -R rate [options]
    
    Generate a rampspec string to ramp to T°C, hold for H hours, and ramp
    back to ambient (25°C), running subtests at fixed intervals throughout."""
    parser.add_option('-T', '--temp', type='float', default=None,
                      help=u'Set target temperature in °C')
    parser.add_option('-H', '--hold', type='float', default=None,
                      help='Set hold time in hours')
    parser.add_option('-R', '--rate', type='float', default=None,
                      help=u'Set temp ramp rate in °C per hour')
    parser.add_option('-W', '--wait', action='store_true',
                      help='Wait until target temp reached')
    parser.add_option('-S', '--stable', action='store_true',
                      help='Wait until temp stable at target')
    parser.add_option('-l', '--limit', type='float', default=1.0,
                      help=u'Tolerance (in °C) for -W,-S')
    parser.add_option('-d', '--dry', action='store_true',
                      help='Activate bedew protection')
    parser.add_option('-j', '--jump', action='store_true',
                      help='End immediately if a test fails')
    parser.add_option('-i', '--interval', type='float',
                      help='Time gap (minutes) between subtests', default=5)
    parser.add_option('-x', '--xdur', type='float',
                      help='Duration (minutes) of subtest', default=0)
    options, args = parser.parse_args()

    if not options.temp:
        sys.stderr.write("ERROR: -T/--temp is required\n")
        sys.exit(2)

    if not options.hold:
        sys.stderr.write("ERROR: -H/--hold is required\n")
        sys.exit(2)

    if not options.rate:
        sys.stderr.write("ERROR: -R/--rate is required\n")
        sys.exit(2)

    if options.xdur >= options.interval:
        sys.stderr.write("ERROR: interval less than test duration!\n")
        sys.exit(2)
    if options.interval - options.xdur < 1:
        sys.stderr.write(
            "WARNING: interval minus test duration is less than 1 minute!\n")

    options.interval /= 60.0 # convert to hours
    options.xdur /= 60.0 # convert to hours

    return options

def ramp_to(temp, options, t):
    ramping = abs((temp - t)/options.rate)
    steps = math.floor(ramping / options.interval)
    stepsize = options.rate * options.interval
    steptime = options.interval - options.xdur
    steprate = abs(stepsize / steptime)
    signum = 1 if temp > t else -1
    j = 'j0' if options.jump else ''
    d = ',d' if options.dry else ''
    s = '[%d#X%s;Rr%f,c%f%s;]' % (steps, j, steprate, stepsize * signum, d)
    s += ('X%s;Rr%f,s%f%s;' % (j, options.rate, temp, d))
    if options.wait:
        s += 'Wl%f%s%s;' % (options.limit, ',z6' if options.stable else '', d)
    return s, temp

def hold_at(temp, hold, options):
    steps = math.floor(hold / options.interval)
    j = 'j0' if options.jump else ''
    d = ',d' if options.dry else ''
    s = '[%d#X%s;Ht%f%s;]' % (steps, j, options.interval - options.xdur, d)
    rest = hold - steps * options.interval
    if rest > 1e-6: # epsilon, because floats can float away
        s += 'Ht%f%s;' % (rest, d)
    return s

if __name__ == '__main__':
    options = parse_cmdline()
    s = 'Ws%f,l0.5;' % AMBIENT
    t = AMBIENT
    ds,t = ramp_to(options.temp, options, t)
    s += ds
    s += hold_at(options.temp, options.hold, options)
    ds,t = ramp_to(AMBIENT, options, t)
    s += ds
    if options.wait:
        s += 'Wt%f,l%f%s%s;' % (AMBIENT, options.limit,
                                ',z6' if options.stable else '',
                                ',d' if options.dry else '')
    s += 'X;'
    print s+'0:I'
