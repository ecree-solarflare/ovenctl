#! /usr/bin/env python
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

import time, optparse, math, sys, operator

def MacroRepeat(args, text):
    count = int(args)
    return count*text

RSActionArgumentTable = (('H', ('c', 'd', 's', 't')), ('I', ('t',)), ('J', ('j',)), ('R', ('c', 'd', 'r', 's', 't')), ('W', ('c', 'd', 'l', 's', 't', 'z')), ('X', ('j',)))
RSActionEnum = tuple(rsaa[0] for rsaa in RSActionArgumentTable)
RSArgumentTypes = (('c', "float"), ('d', "bool"), ('j', "int"), ('l', "float"), ('r', "float"), ('s', "float"), ('t', "float"), ('z', "int"))
RSArgumentEnum = tuple(rsa[0] for rsa in RSArgumentTypes)
RSMacroCalls = (('#', MacroRepeat),)
RSMacroEnum = tuple(rsm[0] for rsm in RSMacroCalls)

class RSParseException(Exception): pass

class RSArgument:
    def __init__(self, arg, value):
        self.arg = arg
        ai = RSArgumentEnum.index(arg)
        argtype = RSArgumentTypes[ai][1]
        if argtype == "bool":
            self.value = True
        elif argtype == "int":
            self.value = int(value)
        elif argtype == "float":
            self.value = float(value)
        else:
            raise Exception("Argument", arg, "has invalid type", argtype)
    def __str__(self):
        ai = RSArgumentEnum.index(self.arg)
        argtype = RSArgumentTypes[ai][1]
        if argtype == "bool":
            return self.arg if self.value else ""
        elif argtype == "int":
            return "%s%d" % (self.arg, self.value)
        elif argtype == "float":
            return "%s%g" % (self.arg, self.value)
        else:
            raise Exception("Argument", self.arg, "has invalid type", argtype)

class RSAction:
    def __init__(self, string):
        self.label = None
        label, colon, rest = string.partition(':')
        if len(colon):
            try:
                self.label = int(label)
            except ValueError:
                raise RSParseException("Invalid label", label, ":", rest)
            string = rest
        self.act = string[0]
        try:
            ai = RSActionEnum.index(self.act)
        except ValueError:
            raise RSParseException("No such action", self.act)
        self.args = []
        argstr = string[1:]
        while len(argstr):
            arg = argstr[0]
            if arg in map(lambda arg: arg.arg, self.args):
                raise RSParseException("Argument", arg, "specified twice")
            if arg not in RSArgumentEnum:
                raise RSParseException("No such argument", arg)
            if arg not in RSActionArgumentTable[ai][1]:
                raise RSParseException("Invalid argument", arg, "for action", self.act)
            value, ignore, argstr = argstr[1:].partition(',')
            self.args.append(RSArgument(arg, value))
        # validate the argument list
        #  R must have at least one of r,t
        if self.act=='R':
            if 'r' not in self and 't' not in self:
                raise RSParseException("Action 'R' (ramp) must have at least one of r, t")
        #  Can't have Wz without l
        elif self.act=='W':
            if 'z' in self and 'l' not in self:
                raise RSParseException("Action 'W' (wait) can't have z without l")
        #  J must have j
        elif self.act=='J' and 'j' not in self:
            raise RSParseException("Action 'J' (jump) must have j")
        #  Can't have s,c anywhere
        elif 's' in self and 'c' in self:
            raise RSParseException("Can't combine s, c")
    def __str__(self):
        return self.act + ','.join(map(str, self.args))
    def __getitem__(self, arg):
        items = filter(lambda a: a.arg==arg, self.args)
        if len(items) > 1:
            raise Exception("Multiple values for argument", arg)
        elif not len(items):
            raise KeyError(arg)
        return items[0].value
    def __contains__(self, arg):
        items = filter(lambda a: a.arg==arg, self.args)
        return len(items) == 1
    def duration(self):
        try:
            return self['t']
        except KeyError:
            return 0
    def setpoint(self, old):
        if 's' in self:
            return self['s']
        elif 'c' in self:
            return old + self['c']
        else:
            return old

def macroexecute(string):
    for i,c in enumerate(string):
        if c in RSMacroEnum:
            n = RSMacroEnum.index(c)
            return(RSMacroCalls[n][1](string[:i], string[i+1:]))
    raise RSParseException("No recognisable macro call in [", string, "]")

def macroexpand(string):
    stack = [[]]
    for c in string:
        if c == '[':
            stack.append([])
        elif c == ']':
            try:
                block = ''.join(stack.pop())
                stack[-1].append(macroexecute(block))
            except IndexError:
                raise RSParseException("Unmatched ']'")
        else:
            stack[-1].append(c)
    if len(stack) > 1:
        raise RSParseException("Unmatched '['")
    return ''.join(stack[0])

class RampSpec:
    def __init__(self, string):
        self.actions = []
        for actstr in macroexpand(string).split(';'):
            if len(actstr):
                self.actions.append(RSAction(actstr))
    def __str__(self):
        return ';'.join(map(str, self.actions))
    def prepare(self, oven, xcallback=None, xcdata=None):
        return RampCtl(self, oven, xcallback, xcdata)

class RampCtl:
    def __init__(self, spec, oven, xcallback=None, xcdata=None):
        self.actions = spec.actions
        self.oven = oven
        self.act_start = time.time()/3600.0
        self.old_setpoint = None
        self.new_action = bool(len(self.actions))
        self.xcallback = xcallback
        self.xcdata = xcdata
    def run(self):
        if not len(self.actions): return 0
        now = time.time()/3600.0
        action = self.actions[0]
        duration = action.duration()
        finished = (now > self.act_start + duration)
        jump_to = None
        if action.act == 'H':
            if self.new_action:
                self.old_setpoint = action.setpoint(self.old_setpoint)
            if self.old_setpoint is not None:
                self.oven.set_setpoint(self.old_setpoint, force=True)
                self.oven.set_mode_active(force=True)
        elif action.act == 'I':
            if self.new_action:
                self.oven.set_mode_idle()
            if finished:
                self.old_setpoint = self.oven.get_temp()
        elif action.act == 'J':
            jump_to = action['j']
        elif action.act == 'R':
            new_setpoint = action.setpoint(self.old_setpoint)
            if 'r' in action:
                if duration:
                    rate = min(abs((new_setpoint-self.old_setpoint)/duration), abs(action['r']))
                else:
                    rate = action['r']
                temp = self.old_setpoint + (now-self.act_start)*math.copysign(rate, new_setpoint-self.old_setpoint)
                finished = ((self.old_setpoint < new_setpoint) != (temp < new_setpoint))
                if finished: temp = new_setpoint
            elif duration:
                tfrac = (now-self.act_start)/duration
                temp = sum(map(operator.mul, (self.old_setpoint, new_setpoint), (1-tfrac, tfrac)))
                if finished: temp = new_setpoint
            else:
                raise Exception("Action 'R' with neither 'r'ate nor 't'ime: %s" % action)
            if finished: self.old_setpoint = temp
            self.oven.set_setpoint(temp, force=True)
            self.oven.set_mode_active(force=True)
        elif action.act == 'W':
            if self.new_action:
                try:
                    self.old_setpoint = action.setpoint(self.old_setpoint)
                except KeyError: pass
            self.oven.set_setpoint(self.old_setpoint, force=True)
            self.oven.set_mode_active(force=True)
            temp = self.oven.get_temp()
            if self.new_action:
                self.old_temp = None
                self.stable = 0
            if 'l' in action and action['l']>0:
                near = abs(self.old_setpoint - temp)<action['l']
            elif self.old_temp is not None:
                near = ((self.old_setpoint < self.old_temp) != (self.old_setpoint < temp))
            else:
                near = False
            if 'z' in action and action['z']>0:
                self.stable = (self.stable + 1) if near else 0
                finished = self.stable > action['z']
            else:
                finished = near
            self.old_temp = temp
        elif action.act == 'X':
            if callable(self.xcallback):
                try:
                    status, self.xcdata = self.xcallback(self.xcdata)
                except (TypeError, ValueError) as err:
                    raise Exception("XCallback didn't return a 2-tuple", err)
                if status:
                    try:
                        jump_to = action['j']
                    except KeyError:
                        jump_to = None
            else:
                raise Exception("XCallback is not callable")
        else:
            raise Exception("Unrecognised action", action.act)
        self.oven.bedew_protection = ('d' in action and action['d'] and self.oven.get_setpoint()<20)
        self.new_action = False
        if finished: self.next()
        if jump_to is not None:
            while len(self.actions):
                if self.actions[0].label == jump_to: break
                self.next()
        return len(self.actions)
    def next(self):
        self.new_action = True
        self.actions = self.actions[1:]
        self.act_start = time.time()/3600.0

def parse_cmdline():
    parser = optparse.OptionParser()
    parser.usage = "%prog -H hostname [-p port] -r rampspec"
    parser.add_option('-H', '--host', help='host to connect to')
    parser.add_option('-p', '--port', help='TCP port to connect to',
                      default=ovenctl.BINDER_PORT)
    parser.add_option('-r', '--rampspec', type='string', 
                      help='Rampspec to follow')
    options, args = parser.parse_args()

    if not options.host:
        print "ERROR: -H/--host is required"
        sys.exit(2)

    if not options.rampspec:
        print "ERROR: -r/--rampspec is required"
        sys.exit(2)

    return options

if __name__ == '__main__':
    import socket, ovenctl
    options = parse_cmdline()
    rs=RampSpec(options.rampspec)
    oven = ovenctl.OvenCtl(options.host, options.port)
    rc=rs.prepare(oven)
    while True:
        if rc.new_action: print "Started action: %s" % rc.actions[0]
        try:
            if not rc.run(): break
        except socket.error: pass
        time.sleep(3)
