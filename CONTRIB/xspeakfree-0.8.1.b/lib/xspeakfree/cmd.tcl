proc GetOS {} {
	set os {unknown-unix}
	catch {set os [exec {uname}]}
	return $os
}

proc timestr {} {
	return [clock format [clock seconds] -format {%m-%d %I:%M %p}]
}

proc Exit {} {
	ControlSpeaker close
	ControlMike close
	exit 0
}

proc Speaker_readevent {} {
	global Speaker

	if [eof $Speaker(exec:stdout)] {
		GUI_print "[timestr]: ERROR: sfspeaker shutdown"
		ControlSpeaker close
	} else {
		gets $Speaker(exec:stdout) line

		if [regexp {([^ ]+) connect$} $line a host] {
			set host [string tolower $host]
			GUI_hostlist add $host -comment {sfspeaker stream} -connection true -transmit true
			GUI_print "[timestr]: $host connected."
			set Speaker(pri:in:$host:protocol) {}
			set Speaker(pri:in:$host:compression) {}
			return
		}
		if [regexp {([^ ]+) sending in (.+)$} $line a host proto] {
			set host [string tolower $host]
			set Speaker(pri:in:$host:protocol) [lreplace $proto end end]
			return
		}
		if [regexp {([^ ]+) sending ([^ ]+) compressed\.$} $line a host compression] {
			set host [string tolower $host]
			set Speaker(pri:in:$host:compression) $compression
			GUI_print "[timestr]: $host sending audio ($Speaker(pri:in:$host:compression)/$Speaker(pri:in:$host:protocol))."
			return
		}
		if { ([regexp {connection closed} $line] && [regexp {sfspeaker: ([^ ]+) } $line a host]) ||
			([regexp {([^ ]+) idle$} $line a host])} {
			set host [string tolower $host]

			set done 0
			foreach n [array names Speaker "pri:in:$host:*"] {
				set done 1
				catch "unset Speaker($n)"
			}
			if {$done == 1} {
				GUI_hostlist edit $host -transmit false
				GUI_print "[timestr]: $host disconnected."
			}
			return
		}
		if {![string first { sending from } $line]} {
			GUI_print "  -- ERROR: sfspeaker:  [string trimleft $line]"
		}
	}
}

proc ControlSpeaker { cmd {spio {}} } {
	global Speaker

	if {$cmd == {restart}} { set cmd {open} }

	if {$Speaker(exec:id) != {} && ($cmd == {open} || $cmd == {close})} {
		close_exec $Speaker(exec:id)
		set Speaker(exec:id) {}
	}

	if {$cmd == {open}} {
		set Speaker(exec:id) [open_exec -command "sfspeaker -v -p$Speaker(port) $Speaker(args)" -stdout Speaker(exec:stdout)]
		fileevent $Speaker(exec:stdout) readable Speaker_readevent
	}
}

proc Mike_readevent {} {
	global Mike

	if [eof $Mike(exec:stdout)] {
		GUI_print "[timestr]: ERROR: sfmike shutdown"
		ControlMike close
	} else {
		if {[gets $Mike(exec:stdout) line] == -1} {
			set line [read $Mike(exec:stdout)]
		}
		if {[string first "Space bar switches" $line] != -1} {
			GUI_print "[timestr]: ready to broadcast to $Mike(target)."
			set Mike(:state:text) "Ready"
			set Mike(pri:ready) 1
			if {$Mike(:state) == 1} {
				set Mike(:state) 0
				ControlMike on
			}
			return
		}
		if {[string first "Pause" $line] != -1} {
			set Mike(:state) 0
			set Mike(:state:text) "Ready"
			return
		}
              if {[string first "Quiet" $line] != -1} {
                      set Mike(:state) 1
                      set Mike(:state:text) "Quiet"
                      return
              }
		if {[string first "Talk" $line] != -1} {
			set Mike(:state) 1
			set Mike(:state:text) "Sending"
			return
		}
		if {$line != {} && $line != "\x0A\x09 \x0A"} {
			GUI_print "  -- ERROR: sfmike:  [string trimleft $line]"
		}
	}
}

proc ControlMike { cmd } {
	global Mike

	if {$cmd == {toggleProc}} {
		if {$Mike(exec:id) == {}} { set cmd {open} } else { set cmd {close} }
	}
	if {$cmd == {restart}} { set cmd {open} }

	if {$Mike(exec:id) != {} && ($cmd == {close} || $cmd == {open})} {
		close_exec $Mike(exec:id)

		set Mike(exec:id) {}

		if {$cmd == {close}} {
			GUI_set_menu_item_state connection close disabled

			set Mike(target) {}
			set Mike(:state) 0
			set Mike(:state:text) "Off"
			set Mike(:button) "Mute"
			set Mike(pri:ready) 0
		}
	}
	if {$cmd == {open} && $Mike(target) != {}} {
		if {$Mike(connect:send_ring) == 1 && $Mike(:state) == 0} {
			set ring [GetSoundPath ring]
			GUI_print "[timestr]: sending ring sound to $Mike(target)."
		} else {
			set ring {}
		}

		set Mike(exec:id) [open_exec -command "sfmike $Mike(args) $Mike(target) $ring ." -stdin Mike(exec:stdin) -stdout Mike(exec:stdout)]
		fconfigure $Mike(exec:stdout) -blocking false
		fileevent $Mike(exec:stdout) readable Mike_readevent
		set Mike(pri:ready) 0

		GUI_set_menu_item_state connection close normal
	}
	if {$Mike(exec:id) != {} && $cmd == {on} && $Mike(pri:ready) == 1} {
		if {$Mike(:state) != 1} {
			puts -nonewline $Mike(exec:stdin) { }
			flush $Mike(exec:stdin)
		}
	}
	if {$Mike(exec:id) != {} && $cmd == {off}} {
		if {$Mike(:state) != 0} {
			puts -nonewline $Mike(exec:stdin) { }
			flush $Mike(exec:stdin)
		}
	}
}

proc ModifyArgs {item} {
	upvar #0 $item i
	
	set i(args) {}
	foreach {n v} [array get i {args:*}] {
		if {[regexp {:eval$} $n]} {
			eval $v
		} else {
			append i(args) { } $v
		}
	}
	if {$i(exec:id) != {}} { $i(command) restart }
}

proc GetSoundPath {file} {
	global env SoundFile DataDir LibDir

	if {[array get SoundFile $file] != {}} {
		set file $SoundFile($file)
	}

	foreach path [list $file [file join $env(HOME) $file] [file join $DataDir $file] [file join $LibDir $file] ] {

		if {[file exists $path]} { return $path }
	}
	
	return {}
}

proc SendSoundFile_readevent {id stdout host snd} {
	if [eof $stdout] {
		close_exec $id

		# remove sound file name from list
		set n [GUI_hostlist get $host -sound]
		set x [string first $snd $n]
		if {$x != -1} {
			set y [string range $n 0 [expr $x - 1]]
			append y [string range $n [expr $x + [string length $snd] + 2] end]
			GUI_hostlist edit $host -sound [string trimright $y {; }]
		}

		# remove execid from list
		regsub "$id ?" [GUI_hostlist varget $host {sendsnd:exec}] {} n
		GUI_hostlist varset $host {sendsnd:exec} $n
	} else {
		set line [read $stdout]
	}
}

proc SendSoundFile { host file {noprint {}} } {
	set path [GetSoundPath $file]
	if {$path == {}} { return 0 }
	if {$noprint == {}} {
		GUI_print "[timestr]: sending $file to $host."
	}
	if {$host == {localhost}} {
		set c {-N}
	} else {
		set c {-T}
	}

	GUI_hostlist edit $host -sound [string trimright "[file tail $path]; [GUI_hostlist get $host -sound]" { ;}]
	set id [open_exec -command "sfmike $host $c $path" -stdout stdout]
	fconfigure $stdout -blocking false
	fileevent $stdout readable "SendSoundFile_readevent $id $stdout $host [file tail $path]"
	GUI_hostlist varset $host {sendsnd:exec} "[GUI_hostlist varget $host {sendsnd:exec}]$id "
	return 1
}

proc InitProcess {} {
	global Speaker SoundFile Mike HostListConstants env

	set SoundFile(busy) {busy.au}
	set SoundFile(ring) {ring.au}

	set Speaker(exec:id) {}
	set Speaker(port) 2074
	set Speaker(args) {}
	set Speaker(args:jitter) { }
	set Speaker(args:disable_remote_ring) { }
	set Speaker(args:user) {}
	set Speaker(command) ControlSpeaker
	set Speaker(:record:file) "$env(HOME)/sfspeaker_audio.next"
	set Speaker(:record:append) {}
	set Speaker(:record:onoff) 0

	set Speaker(args:record:eval) {
		if {$i(:record:onoff) == 1 && $i(:record:file) != {}} {
			append i(args) " -R$i(:record:append)$i(:record:file)"
		} else {
			set i(:record:onoff) 0
		}
	}
	set Speaker(:crypt:method) { }
	set Speaker(args:crypt:eval) {
		global Speaker
		if {$Speaker(:crypt:method) != { }} {
			append i(args) "$Speaker(:crypt:method)$Speaker(:crypt:$Speaker(:crypt:method))"
		}
	}
	set Mike(args:crypt:eval) $Speaker(args:crypt:eval)

	set Mike(exec:id) {}
	set Mike(target) {}
	set Mike(args) {}
	set Mike(command) ControlMike
	set Mike(args:simplecompression) { }
	set Mike(args:compression) {-T}
	set Mike(args:protocol) { }
	set Mike(args:user) {}
	set Mike(args:ring) { }

	set Mike(connect:send_ring) 0
	
	set Mike(hostlist:add:send_busy) 0
	set Mike(hostlist:add:play_ring) 0

	set Mike(:state) 0
	set Mike(:state:text) {Off}
	set Mike(:button) {Mute}

	set HostListConstants [list]
}

set xspeakfree_rc_path "$env(HOME)/.xspeakfree.rc"
set xspeakfree_hostlist_path  "$env(HOME)/.xspeakfree.hostlist"

#Handle command line args
for {set llen [llength $argv]; set i 0} {$i < $llen} {incr i} {
	switch -glob -- [lindex $argv $i] {
		-[fF]* { regexp -nocase -- {-f(.*)} [lindex $argv $i] a xspeakfree_rc_path }
		-[pP]* { regexp -nocase -- {-p(.*)} [lindex $argv $i] a Speaker(port) }
		-[nN] { set Speaker(args:disable_remote_ring) {-n} }
		-? -
		-[uU] -
		default {
			puts {usage: xspeakfree [-n][-p<port>][-f<path>]

	-f<path>	use different xspeakfree.rc than
				$HOME/.xspeakfree.rc
	-n		disable remote ring
	-p<port>	force sfspeaker to listen on port number <port>
	-u		this help message
}
			exit
		}
	}
}

InitProcess
GUI_draw
GUI_help_about

# source siteconf.tcl
source [file join $LibDir siteconf.tcl]

# Load user's .xspeakfree.rc
if [catch { source $xspeakfree_rc_path } error] {
	if {$error != "couldn't read file \"$xspeakfree_rc_path\": no such file or directory"} {
		GUI_print "ERROR: in $xspeakfree_rc_path"
		GUI_print "    " $error
		GUI_print "--------------------------"
	} else {
		GUI_print "Could not load $xspeakfree_rc_path"
	}
} else {
	GUI_print "$xspeakfree_rc_path loaded"
}
unset error
unset xspeakfree_rc_path

# Load user's .xspeakfree.hostlist
if {[catch {set fd [open $xspeakfree_hostlist_path {r}]} error]} {
	if {[catch {set fd [open $xspeakfree_hostlist_path {w}]} error]} {
		GUI_print "Could not create $xspeakfree_hostlist_path: $error"
	} else {
		puts $fd "# xspeakfree host list
# this file contains the default values for the target host list popup
# its format is IP or DNS name of the host followed by whitespace and then
# a comment field that lasts until the end of the line.
# ie:
# echo.fourmilab.ch	Echo Server
#
"
		foreach {n l} $HostList_Defaults {
			puts $fd "$n	$l"
			lappend HostListConstants $n $l
			GUI_hostlist add $n -comment $l
		}
		close $fd
		GUI_print "Created $xspeakfree_hostlist_path"
	}
} else {
	while {[gets $fd line] > -1} {
		if {![regexp {^#} $line] && [string length $line] > 1} {
			lappend HostListConstants [lindex $line 0] [lrange $line 1 end]
			GUI_hostlist add [lindex $line 0] -comment [lrange $line 1 end]
		}
	}
	close $fd
	GUI_print "$xspeakfree_hostlist_path loaded"
}
unset xspeakfree_hostlist_path
unset error

ModifyArgs Speaker
ModifyArgs Mike
ControlSpeaker open
