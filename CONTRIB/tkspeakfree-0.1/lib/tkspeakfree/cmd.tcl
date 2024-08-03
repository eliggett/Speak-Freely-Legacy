proc GetOS {} {
    set os {unknown-unix}
    catch {set os [exec {uname}]}
    return $os
}

proc timestr {} {
    return [clock format [clock seconds] -format {%m-%d %I:%M %p}]
}

proc Exit {} {
    ControlLauncher close
    exit 0
}

proc Launcher_readevent {} {
    global Launcher

    if [eof $Launcher(exec:stdout)] {
	GUI_print "[timestr]: ERROR: sflaunch shutdown"
	ControlLauncher close
    } else {
	if {[gets $Launcher(exec:stdout) line] == -1} {
	    # GUI_print "raw: $line"
	    set line [read $Launcher(exec:stdout)]
	}

	# GUI_print "raw: $line"

	# Speaker follows

	if [regexp {([^ ]+) connect$} $line a host] {
	    set host [string tolower $host]
	    GUI_hostlist add $host -comment {sflaunch stream} -connection true -transmit true
	    GUI_print "[timestr]: $host connected."
	    set Launcher(pri:in:$host:protocol) {}
	    set Launcher(pri:in:$host:compression) {}
	    return
	}
	if [regexp {([^ ]+) ([^ ]+) \(([^ ]+)\) sending from (.+)$} $line a cname fname email host] {
	    set host [string tolower $host]
	    GUI_print "$cname $fname speaking from $host "
	    GUI_print "  contact: $cname $fname <$email>"
	    return
	}
	if [regexp {([^ ]+) sending in (.+)$} $line a host proto] {
	    set host [string tolower $host]
	    set Launcher(pri:in:$host:protocol) [lreplace $proto end end]
	    return
	}
	if [regexp {([^ ]+) sending ([^ ]+) compressed\.$} $line a host compression] {
	    set host [string tolower $host]
	    set Launcher(pri:in:$host:compression) $compression
	    GUI_print "[timestr]: $host sending audio ($Launcher(pri:in:$host:compression)/$Launcher(pri:in:$host:protocol))."
	    return
	}
	if { ([regexp {connection closed} $line] && [regexp {sfspeaker: ([^ ]+) } $line a host]) ||
	([regexp {([^ ]+) idle$} $line a host])} {
	    set host [string tolower $host]

	    set done 0
	    foreach n [array names Launcher "pri:in:$host:*"] {
		set done 1
		catch "unset Launcher($n)"
	    }
	    if {$done == 1} {
		GUI_hostlist edit $host -transmit false
		GUI_print "[timestr]: $host disconnected."
	    }
	    return
	}
#	if {![string first { sending from } $line]} {
#	    GUI_print "  -- ERROR: sflaunch:  [string trimleft $line]"
#	}

	# Speaker end

	if {[string first "Space bar switches" $line] != -1} {
	    GUI_print "[timestr]: ready to broadcast to $Launcher(target)."
	    set Launcher(:state:text) "Connected"
	    set Launcher(:state) 2
	    set Launcher(pri:ready) 1
	    return
	}
	if {[string first "Pause" $line] != -1} {
	    set Launcher(:state) 2
	    set Launcher(:state:text) "Connected"
	    return
	}
	if {[string first "Talk" $line] != -1} {
	    set Launcher(:state) 3
	    set Launcher(:state:text) "Sending"
	    return
	}

	# new stuff
	if {[string first "New" $line] != -1} {
	    set Launcher(:state) 1
	    set Launcher(:state:text) "Ready"

	    return
	}
	if {[string first "sflaunch cannot open one or both" $line] != -1} {
	    GUI_print "Launcher Weg!"
	    return
	}
	if [regexp {Incoming Chat: (.+)$} $line a chat] {
	    GUI_print $chat
	    return
	}
	if {[string first "Chat:" $line] != -1} {
	    set text [.chat.v get 1.0 end]
	    set text [string trim $text]
	    puts $Launcher(exec:stdin) $text
	    flush $Launcher(exec:stdin)
	    .chat.v delete 1.0 end
	    return
	}
	# end new stuff


	if {$line != {} && $line != "\x0A\x09 \x0A"} {
	    GUI_print "  -- ERROR:  [string trimleft $line]"
	}
    }
}

proc LauncherChat { } {
    global Launcher

    if {$Launcher(:state) == 2} {
	puts -nonewline $Launcher(exec:stdin) {.}
	flush $Launcher(exec:stdin)
    } else {
	GUI_print "input ignored: not in connected mode!"
    }
}

proc LauncherCommand { cmd } {
    global Launcher 

    puts "  cmd: $cmd"

    if { $cmd == "close" } {
	puts $Launcher(exec:stdin) {}
    }
    if { $cmd == "connect" } {
	set host  $Launcher(target)
	puts $Launcher(exec:stdin) $host
    }
    if { $cmd == "disconnect" } {
	puts -nonewline $Launcher(exec:stdin) {q}
    }
    if { $cmd == "speak" } {
	puts -nonewline $Launcher(exec:stdin) { }
    }
    if { $cmd == "hangup" } {
	puts -nonewline $Launcher(exec:stdin) { }
    }
    flush $Launcher(exec:stdin)
}

proc ControlLauncher { cmd } {
    global Launcher

    puts "gcmd: $cmd"

    if {$cmd == {toggleProc}} {
	if {$Launcher(exec:id) == {}} { 
	    set cmd {open} 
	} else { 
	    set cmd {close} 
	}
    }

    if {$cmd == {restart}} { set cmd {open} }

    if {$cmd == {open}} { 
	if {$Launcher(:state) >= 3} { # speaking
	    LauncherCommand hangup
	}
	if {$Launcher(:state) >= 2} { # connected
	    LauncherCommand disconnect
	}
	if {$Launcher(:state) >= 1} { # opened
	    LauncherCommand close
	}
	
	if {$Launcher(exec:id) != {}} {
	    close_exec $Launcher(exec:id)
	    set Launcher(exec:id) {}
	}

	set Launcher(exec:id) [open_exec -command "sflaunch -speaker -v $Launcher(sargs) -mike $Launcher(margs) $Launcher(target) " -stdin Launcher(exec:stdin) -stdout Launcher(exec:stdout)]
	fconfigure $Launcher(exec:stdout) -blocking false
	fileevent $Launcher(exec:stdout) readable Launcher_readevent
	set Launcher(pri:ready) 0

	if {$Launcher(:state) == 0} { # was closed
	    # open it
	    set Launcher(:state) 1
	}	
	if {$Launcher(:state) >= 2} { # connected
	    LauncherCommand connect
	}
	if {$Launcher(:state) >= 3} { # speaking
	    LauncherCommand speak
	}
    }

    if {$cmd == {close}} { 
	if {$Launcher(:state) >= 3} { # speaking
	    LauncherCommand hangup
	}
	if {$Launcher(:state) >= 2} { # connected
	    LauncherCommand disconnect
	}
	if {$Launcher(:state) >= 1} { # opened
	    LauncherCommand close
	}
	# close it
	set Launcher(:state) 0
	if {$Launcher(exec:id) != {}} {
	    close_exec $Launcher(exec:id)
	    set Launcher(exec:id) {}
	    
	    GUI_set_menu_item_state connection close disabled
	    
	    set Launcher(target) {}
	    set Launcher(:state) 0
	    set Launcher(:state:text) ""
	    set Launcher(:button) "Mute"
	    set Launcher(pri:ready) 0
	}
    }

    if {$cmd == {connect}} { 
	if {$Launcher(:state) >= 3} { # speaking
	    LauncherCommand hangup
	}
	if {$Launcher(:state) >= 2} { # connected
	    LauncherCommand disconnect
	}
	if {$Launcher(:state) == 0} { # was closed
	    LauncherCommand open
	}	
	LauncherCommand connect
	if {$Launcher(:state) < 2} { # was connected?
	    # connect it
	    set Launcher(:state) 2
	    GUI_set_menu_item_state connection close normal
	}
	if {$Launcher(:state) >= 3} { # speaking
	    LauncherCommand speak
	}
    }

    if {$cmd == {disconnect}} { 
	if {$Launcher(:state) >= 3} { # speaking
	    LauncherCommand hangup
	}
	if {$Launcher(:state) >= 2} { # connected
	    LauncherCommand disconnect
	    # disconnect it
	    set Launcher(:state) 1
	    set Launcher(target) {}
	    GUI_set_menu_item_state connection close disabled
	}
    }

    if {$cmd == {speak}} { 
	if {$Launcher(:state) < 3} { # not speaking
	    LauncherCommand speak
	}
    }

    if {$cmd == {hangup}} { 
	if {$Launcher(:state) >= 3} { # speaking
	    LauncherCommand hangup
	    # hangup
	    set Launcher(:state) 2
	}
    }
    
    puts "done gcmd"
}

proc ModifyArgs {item} {
    upvar #0 $item i
    
    set i(margs) {}
    foreach {n v} [array get i {margs:*}] {
	if {[regexp {:eval$} $n]} {
	    eval $v
	} else {
	    append i(margs) { } $v
	}
    }
    set i(sargs) {}
    foreach {n v} [array get i {sargs:*}] {
	if {[regexp {:eval$} $n]} {
	    eval $v
	} else {
	    append i(sargs) { } $v
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

proc SendSoundFile { host c file {noprint {}} } {
    set path [GetSoundPath $file]
    if {$path == {}} { return 0 }
    if {$noprint == {}} {
	GUI_print "[timestr]: sending $file to $host."
    }
    GUI_hostlist edit $host -sound [string trimright "[file tail $path]; [GUI_hostlist get $host -sound]" { ;}]

    set id [open_exec -command "sfmike $host $c $path" -stdout stdout]
    fconfigure $stdout -blocking false
    fileevent $stdout readable "SendSoundFile_readevent $id $stdout $host [file tail $path]"
    GUI_hostlist varset $host {sendsnd:exec} "[GUI_hostlist varget $host {sendsnd:exec}]$id "
    return 1
}

proc InitProcess {} {
    global SoundFile Launcher HostListConstants env

    set SoundFile(busy) {busy.au}
    set SoundFile(ring) {ring.au}

    set Launcher(sargs) {}
    set Launcher(sargs:jitter) {-J1000}
    set Launcher(sargs:disable_remote_ring) { }
    set Launcher(sargs:user) {}
    set Launcher(:record:file) "$env(HOME)/sfLauncher_audio.next"
    set Launcher(:record:append) {}
    set Launcher(:record:onoff) 0

    set Launcher(sargs:record:eval) {
	if {$i(:record:onoff) == 1 && $i(:record:file) != {}} {
	    append i(sargs) " -R$i(:record:append)$i(:record:file)"
	} else {
	    set i(:record:onoff) 0
	}
    }
    set Launcher(:crypt:method) { }
    set Launcher(sargs:crypt:eval) {
	global Launcher
	if {$Launcher(:crypt:method) != { }} {
	    append i(sargs) "$Launcher(:crypt:method)$Launcher(:crypt:$Launcher(:crypt:method))"
	}
    }
    set Launcher(margs:crypt:eval) $Launcher(sargs:crypt:eval)

    set Launcher(exec:id) {}
    set Launcher(target) {}
    set Launcher(margs) {}
    set Launcher(command) ControlLauncher
    set Launcher(margs:simplecompression) { }
    set Launcher(margs:compression) {-T}
    set Launcher(margs:protocol) { }
    set Launcher(margs:user) {}
    set Launcher(margs:ring) { }

    set Launcher(connect:send_ring) 0
    
    set Launcher(hostlist:add:send_busy) 0
    set Launcher(hostlist:add:play_ring) 0

    set Launcher(:state) 0
    set Launcher(:state:text) {Off}
    set Launcher(:button) {Mute}

    set HostListConstants [list]
}

set tkspeakfree_rc_path "$env(HOME)/.tkspeakfree.rc"
set tkspeakfree_hostlist_path  "$env(HOME)/.tkspeakfree.hostlist"

#Handle command line args
for {set llen [llength $argv]; set i 0} {$i < $llen} {incr i} {
    switch -glob -- [lindex $argv $i] {
	-[fF]* { regexp -nocase -- {-f(.*)} [lindex $argv $i] a tkspeakfree_rc_path }
	-[nN] { set Launcher(args:disable_remote_ring) {-n} }
	-? -
	-[uU] -
	default {
	    puts {usage: tkspeakfree [-n][-p<port>][-f<path>]

	    -f<path>	use different tkspeakfree.rc than
	    $HOME/.tkspeakfree.rc
	    -n		disable remote ring
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

# Load user's .tkspeakfree.rc
if [catch { source $tkspeakfree_rc_path } error] {
    if {$error != "couldn't read file \"$tkspeakfree_rc_path\": no such file or directory"} {
	GUI_print "ERROR: in $tkspeakfree_rc_path"
	GUI_print "    " $error
	GUI_print "--------------------------"
    } else {
	GUI_print "Could not load $tkspeakfree_rc_path"
    }
} else {
    GUI_print "$tkspeakfree_rc_path loaded"
}
unset error
unset tkspeakfree_rc_path

# Load user's .tkspeakfree.hostlist
if {[catch {set fd [open $tkspeakfree_hostlist_path {r}]} error]} {
    if {[catch {set fd [open $tkspeakfree_hostlist_path {w}]} error]} {
	GUI_print "Could not create $tkspeakfree_hostlist_path: $error"
    } else {
	puts $fd "# tkspeakfree host list
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
	GUI_print "Created $tkspeakfree_hostlist_path"
    }
} else {
    while {[gets $fd line] > -1} {
	if {![regexp {^#} $line] && [string length $line] > 1} {
	    lappend HostListConstants [lindex $line 0] [lrange $line 1 end]
	    GUI_hostlist add [lindex $line 0] -comment [lrange $line 1 end]
	}
    }
    close $fd
    GUI_print "$tkspeakfree_hostlist_path loaded"
}
unset tkspeakfree_hostlist_path
unset error

exec sleep 1

ModifyArgs Launcher

ControlLauncher open
