proc GUI_print {args} {
	global GUI_status
	
	regsub -all "\t" [join $args] {    } x

	foreach i [split $x "\n"] {
		$GUI_status insert end $i
	}
	$GUI_status see end
}

proc GUI_askLWL { } {
    set name [GUI_askdialog .askdialog {Send Audio to Remote Host} {Send Audio to Remote Host:} 20 {}]
}

proc GUI_connection_new { {noconnect {}} } {
	global Launcher GUI_hostlist_l

	if {$noconnect == {}} {
		set host [GUI_askdialog .askdialog {Send Audio to Remote Host} {Send Audio to Remote Host:} 20 {}]
	} else {
		set host [GUI_askdialog .askdialog {Add Host} {Add Host:} 20 {}]
	}
	set host [string trim $host]

	if {$host != {} && $host != $Launcher(target)} {
		GUI_hostlist add $host
		if {$noconnect == {}} {
			$GUI_hostlist_l selection clear 0 end
			$GUI_hostlist_l selection set end
			set Launcher(target) $host
			ControlLauncher connect
		}
	}
}

proc GUI_hostlist {cmd {host {}} args} {
	global GUI_hostlist_l Launcher HostListConstants GUI_hostlist_values
	
	if {$cmd == {clear}} {
		unset GUI_hostlist_values
		$GUI_hostlist_l delete 0 end

		foreach {n l} $HostListConstants {
			GUI_hostlist add $n -comment $l
		}

		return
	}

	if {$cmd == {get}} {
		set n [string trim $args {- }]
		return $GUI_hostlist_values($host:$n)
	}
	if {$cmd == {varget}} {
		set r {}
		catch {set r $GUI_hostlist_values($host:[string trim $args])}
		return $r
	}
	if {$cmd == {varset}} {
		return [set GUI_hostlist_values($host:[string trim [lindex $args 0]]) [lindex $args 1]]
	}
	if {$cmd == {host}} {
		return [lindex [string trimleft [$GUI_hostlist_l get $host] { +}] 0]
	}

	set comment {}; set comments 0
	set xmit {}; set xmits 0
	set sound {}; set sounds 0
	set connection false; set connections 0
	foreach {n v} $args {
		switch -exact -- $n {
			-comment {set comment $v; set comments 1}
			-transmit {set xmit $v; set xmits 1}
			-sound {set sound $v; set sounds 1}
			-connection {set connection $v; set connections 1}
		}
	}

	set host [string tolower $host]

	if {[array get GUI_hostlist_values "$host:*"] != {}} {
		if {$cmd == {add}} { set cmd {edit} }

		if {!$comments} { set comment $GUI_hostlist_values($host:comment) }
		if {!$sounds} { set sound $GUI_hostlist_values($host:sound) }
		if {!$xmits} { set xmit $GUI_hostlist_values($host:xmit) }
	}

	if {$xmit == {+} || $xmit == {true}} { set xmit {+} } else { set xmit { } }

	set label [format {%-1.1s %-30.30s  %-20.20s  %-20.20s} $xmit $host $comment $sound]

	set GUI_hostlist_values($host:comment) $comment
	set GUI_hostlist_values($host:sound) $sound
	set GUI_hostlist_values($host:xmit) $xmit

	if {$cmd == {edit}} {
		for {set i 0; set end [$GUI_hostlist_l size]} {$i < $end} {incr i} {
			if {[lindex [string trimleft [$GUI_hostlist_l get $i] { +}] 0] == $host} {
				$GUI_hostlist_l delete $i
				$GUI_hostlist_l insert $i $label
			}
		}
	}

	if {$cmd == {add}} {
		set insert 1

		for {set i 0; set end [$GUI_hostlist_l size]} {$i < $end} {incr i} {
			if {[lindex [string trimleft [$GUI_hostlist_l get $i] { +}] 0] == $host} { set insert 0 }
		}

		if {$insert == 1} {
			$GUI_hostlist_l insert end $label
		}
		if {$connection} {
			if {$Launcher(hostlist:add:send_busy) == 1 && $Launcher(target) != {}} {
				SendSoundFile $host $Launcher(margs:compression) busy
			} else {
				if {$host != {localhost} && $Launcher(hostlist:add:play_ring) == 1} {
					SendSoundFile localhost $Launcher(margs:compression) ring noprint
				}
			}
		}
	}

	if {$cmd == {remove}} {
		if {$Launcher(target) == $host} { return }

		foreach n [array names GUI_hostlist_values "$host:*"] {
			catch {unset GUI_hostlist_values($n) }
		}
		
		foreach {n l} $HostListConstants { if {$n == $host} { return } }

		for {set i 0; set end [$GUI_hostlist_l size]} {$i < $end} {incr i} {
			if {[lindex [string trimleft [$GUI_hostlist_l get $i] { +}] 0] == $host} {
				$GUI_hostlist_l delete $i
			}
		}
	}
}

proc GUI_set_menu_item_state { menu item state } {
	global GUI_info
	
	foreach {m id} $GUI_info($menu:$item) {
		$m entryconfigure $id -state $state
	}
}

proc GUI_draw {} {
	global GUI_hostlist_l GUI_info GUI_status HostListConstants GUI_hostlist_label

	wm title . TkSpeakFreely
	bind . <Destroy> {Exit}
	wm protocol . WM_DELETE_WINDOW {destroy .}

	set fixed1 {-*-fixed-*-*-*-*-10-*-*-*-*-*-*-*}

	frame .menubar -relief raised -border 2
	pack .menubar -fill x

	set m .menubar.connection.m
	menubutton .menubar.connection -text "Connection" -underline 0 -menu $m
	menu $m -tearoff 0
	$m add command -label "New..." -command GUI_connection_new
	$m add command -label "Close" -command  {
		ControlLauncher disconnect
		$GUI_hostlist_l selection clear 0 end	
		} -state disabled
	$m add command -label "Restart sfspeaker" -command { ControlLauncher open }
	$m add separator
	$m add command -label "Send audio file..." -command {
		set file [tk_getOpenFile]
		if {$file != {}} {
		    set host $Launcher(target)
		    if {$host == {}} {
			set host "localhost"
			GUI_hostlist add localhost -connection false -transmit false
		    }

		    set file [exec basename $file]

		    SendSoundFile $host $Launcher(margs:compression) $file
		}
	}
	$m add separator
	$m add command -label "Clear Status" -command { $GUI_status delete 0 end }
	$m add command -label "Clear Target Host List" -command { GUI_hostlist clear }
	$m add separator
	$m add command -label "Exit" -command Exit
	pack .menubar.connection -side left

	lappend GUI_info(connection:close) $m 1

	set m .menubar.options.m
	menubutton .menubar.options -text "Options" -underline 0 -menu $m
	menu $m -tearoff 0
#	$m add cascade -label "Compression" -menu .menubar.options.m.compression
	$m add checkbutton -label "Simple Compression" -command {ModifyArgs Launcher} -onvalue {-C} -offvalue { } -variable Launcher(margs:simplecompression)
	$m add separator
	$m add radiobutton -label "No Compression" -command {ModifyArgs Launcher} -value {-N} -variable Launcher(margs:compression)
	$m add radiobutton -label "GSM Compression" -command {ModifyArgs Launcher} -value {-T} -variable Launcher(margs:compression)
	$m add radiobutton -label "ADPCM Compression" -command {ModifyArgs Launcher} -value {-F} -variable Launcher(margs:compression)
	$m add radiobutton -label "LPC Compression" -command {ModifyArgs Launcher} -value {-LPC} -variable Launcher(margs:compression)
	$m add radiobutton -label "LPC-10 1x Compression" -command {ModifyArgs Launcher} -value {-LPC10R1} -variable Launcher(margs:compression)
	$m add radiobutton -label "LPC-10 2x Compression" -command {ModifyArgs Launcher} -value {-LPC10R2} -variable Launcher(margs:compression)
	$m add radiobutton -label "LPC-10 3x Compression" -command {ModifyArgs Launcher} -value {-LPC10R3} -variable Launcher(margs:compression)
	$m add radiobutton -label "LPC-10 4x Compression" -command {ModifyArgs Launcher} -value {-LPC10R4} -variable Launcher(margs:compression)
	$m add separator
	$m add cascade -label "Transmission Protocol" -menu .menubar.options.m.protocol
	$m add checkbutton -label "Send Remote Ring" -command {ModifyArgs Launcher} -onvalue {-R} -offvalue { } -variable Launcher(margs:ring)
	$m add command -label "Additional sfmike Arguments..." -command {
		set f [GUI_askdialog .askdialog {sfmike arguments} {Additional sfmike arguments} 20 $Launcher(margs:user)]
		if {$f != $Launcher(margs:user)} {
			set Launcher(margs:user) $f
			ModifyArgs Launcher
		}
	}
	$m add separator
	$m add command -label "Encryption Options..." -command {
		if {[newdialog -path .askdialog -title {Encryption Options} -create {
			global Launcher

			label $w.l -text "Encryption Options"
			pack $w.l -ipadx 10 -ipady 10 -side top

			frame $w.f1
			label $w.f1.l -text "Encryption Method:"
			radiobutton $w.f1.r0 -text "None" -value { } -variable Launcher(:crypt:method)
			radiobutton $w.f1.r1 -text "Blowfish" -value {-BF} -variable Launcher(:crypt:method)
			radiobutton $w.f1.r2 -text "DES" -value {-K} -variable Launcher(:crypt:method)
			radiobutton $w.f1.r3 -text "File" -value {-O} -variable Launcher(:crypt:method)
			radiobutton $w.f1.r4 -text "PGP" -value {-Z} -variable Launcher(:crypt:method)
			pack $w.f1.l -side left -anchor w
			pack $w.f1.r0 -side left -anchor w
			pack $w.f1.r1 -side left -anchor w
			pack $w.f1.r2 -side left -anchor w
			pack $w.f1.r3 -side left -anchor w
			pack $w.f1.r4 -side left -anchor w
			pack $w.f1 -anchor w
			
			frame $w.f2
			frame $w.f2.fl
			frame $w.f2.fr
			
			label $w.f2.fl.l1 -text "Blowfish Key:"
			pack $w.f2.fl.l1 -anchor w
			label $w.f2.fl.l2 -text "DES Key:"
			pack $w.f2.fl.l2 -anchor w
			label $w.f2.fl.l3 -text "File:"
			pack $w.f2.fl.l3 -anchor w
			label $w.f2.fl.l4 -text "PGP Pass Phrase:"
			pack $w.f2.fl.l4 -anchor w
			
			entry $w.f2.fr.e1 -textvariable Launcher(:crypt:-BF) -width 40
			pack $w.f2.fr.e1 -anchor w
			entry $w.f2.fr.e2 -textvariable Launcher(:crypt:-K) -width 40
			pack $w.f2.fr.e2 -anchor w
			frame $w.f2.fr.f1
			entry $w.f2.fr.f1.e3 -textvariable Launcher(:crypt:-O) -width 25
			button $w.f2.fr.f1.b1 -text "Select File" -command {
					set f [ tk_getOpenFile -title {Crypt file:} -initialfile [file tail $Launcher(:crypt:-O)] -initialdir [file dirname $Launcher(:crypt:-O)] ]
					if {$f != {}} {
						set Launcher(:crypt:-O) $f
					}
				}
			pack $w.f2.fr.f1.e3 -side left -anchor w
			pack $w.f2.fr.f1.b1 -side left -anchor w
			pack $w.f2.fr.f1 -anchor w
			entry $w.f2.fr.e4 -textvariable Launcher(:crypt:-Z) -width 40
			pack $w.f2.fr.e4 -anchor w
			
			pack $w.f2.fl -side left
			pack $w.f2.fr -side right
			pack $w.f2

			button $w.b1 -text "Ok" -command {set GUI_pri_buttonValue 1}
			pack $w.b1 -side bottom -anchor se

		} -variable GUI_pri_buttonValue] == 1} {
			ModifyArgs Launcher
		}
	}
	$m add cascade -label "Jitter Compensation" -menu .menubar.options.m.jitter
	$m add checkbutton -label "Disable Remote Ring Requests" -command {ModifyArgs Launcher} -onvalue {-n} -offvalue { } -variable Launcher(sargs:disable_remote_ring)
	$m add command -label "Record Audio To File ..." -command {
		if {[newdialog -path .askdialog -title {Record Audio To File} -create {
				global Launcher

				label $w.l -text "Record Audio To File"
				pack $w.l -ipadx 10 -ipady 10 -side top
				
				frame $w.f3
				label $w.f3.l -text "Record Audio:"
				radiobutton $w.f3.r1 -text "Yes" -value 1 -variable Launcher(:record:onoff) -command "
					$w.f2.r1 configure -state normal
					$w.f2.r2 configure -state normal
					$w.f.b configure -state normal
					$w.f.v configure -fg black
					$w.f.l configure -fg black
				"
				radiobutton $w.f3.r2 -text "No" -value 0 -variable Launcher(:record:onoff) -command "
					$w.f2.r1 configure -state disabled
					$w.f2.r2 configure -state disabled
					$w.f.b configure -state disabled
					$w.f.v configure -fg grey
					$w.f.l configure -fg grey
				"
				pack $w.f3.l -side left -anchor w
				pack $w.f3.r1 -side left -anchor w
				pack $w.f3.r2 -side left -anchor w
				pack $w.f3
				
				frame $w.fp1
				pack $w.fp1 -ipady 20

				frame $w.f
				label $w.f.l -text "Record into:"
				pack $w.f.l -side left -anchor w
				label $w.f.v -textvariable Launcher(:record:file)
				pack $w.f.v -side left -anchor w

				button $w.f.b -text "Select File" -command {
					set f [ tk_getSaveFile -title {Record into:} -initialfile [file tail $Launcher(:record:file)] -initialdir [file dirname $Launcher(:record:file)] ]
					if {$f != {}} {
						set Launcher(:record:file) $f
					}
				}
				pack $w.f.b -side right -anchor w
				pack $w.f

				frame $w.f2
				radiobutton $w.f2.r1 -text "Overwrite file" -value {} -variable Launcher(:record:append)
				radiobutton $w.f2.r2 -text "Append to file" -value {+} -variable Launcher(:record:append)
				pack $w.f2.r1 -side right -anchor w
				pack $w.f2.r2 -side right -anchor w
				pack $w.f2

				frame $w.fp
				pack $w.fp -ipady 20

				if {$Launcher(:record:onoff) == 0} {
					$w.f2.r1 configure -state disabled
					$w.f2.r2 configure -state disabled
					$w.f.b configure -state disabled
					$w.f.v configure -fg grey
					$w.f.l configure -fg grey
				}

				
				frame $w.fb
				button $w.fb.b1 -text "Ok" -command { 
					if {$Launcher(:record:file) == {}} {
						set GUI_pri_buttonValue 2
					} else {
						set GUI_pri_buttonValue 1
					}
				}
				button $w.fb.b2 -text "Cancel" -command { set GUI_pri_buttonValue 2 }
				pack $w.fb.b1 -anchor se -side right
				pack $w.fb.b2 -anchor sw -side left
				pack $w.fb

				set buttonValue 0
			} -variable GUI_pri_buttonValue] == 1} {
			ModifyArgs Launcher
		}
	}
	$m add command -label "Additional sfspeaker Arguments..." -command {
		set f [GUI_askdialog .askdialog {sfspeaker arguments} {Additional sfspeaker arguments} 20 $Launcher(sargs:user)]
		if {$f != $Launcher(sargs:user)} {
			set Launcher(sargs:user) $f
			ModifyArgs Launcher
		}
	}
	$m add separator
	$m add checkbutton -label "Send Ring Sound To Targets" -onvalue 1 -offvalue 0 -variable Launcher(connect:send_ring)
	$m add separator
	$m add checkbutton -label "Sound Ring For New Connections" -onvalue 1 -offvalue 0 -variable Launcher(hostlist:add:play_ring)
	$m add checkbutton -label "Send Busy Signal If Already Connected" -onvalue 1 -offvalue 0 -variable Launcher(hostlist:add:send_busy)
	pack .menubar.options -side left

#	set m .menubar.options.m.compression
#	menu $m -tearoff 0

	set m .menubar.options.m.jitter
	menu $m -tearoff 0
	$m add radiobutton -label "None" -command {ModifyArgs Launcher} -value " " -variable Launcher(sargs:jitter)
	$m add separator
	$m add radiobutton -label "1/10 Second" -command {ModifyArgs Launcher} -value "-J100" -variable Launcher(sargs:jitter)
	$m add radiobutton -label "1/4 Second" -command {ModifyArgs Launcher} -value "-J250" -variable Launcher(sargs:jitter)
	$m add radiobutton -label "1/2 Second" -command {ModifyArgs Launcher} -value "-J500" -variable Launcher(sargs:jitter)
	$m add radiobutton -label "3/4 Second" -command {ModifyArgs Launcher} -value "-J750" -variable Launcher(sargs:jitter)
	$m add radiobutton -label "1 Second" -command {ModifyArgs Launcher} -value "-J1000" -variable Launcher(sargs:jitter)
	$m add radiobutton -label "2 Seconds" -command {ModifyArgs Launcher} -value "-J2000" -variable Launcher(sargs:jitter)
	$m add radiobutton -label "3 Seconds" -command {ModifyArgs Launcher} -value "-J3000" -variable Launcher(sargs:jitter)

	set m .menubar.options.m.protocol
	menu $m -tearoff 0
	$m add radiobutton -label "Speak Freely" -command {ModifyArgs Launcher} -value " " -variable Launcher(margs:protocol)
	$m add radiobutton -label "Real Time Protocol (RTP)" -command {ModifyArgs Launcher} -value "-RTP" -variable Launcher(margs:protocol)
	$m add radiobutton -label "Visual Audio Tool (VAT)" -command {ModifyArgs Launcher} -value "-VAT" -variable Launcher(margs:protocol)

	set m .menubar.lwl.m
	menubutton .menubar.lwl -text "Who's Listening" -underline 0 -menu $m
	menu $m -tearoff 0
	$m add command -label "Look Who's Listening" -command {
	    set pattern [GUI_askdialog .askdialog {Word to search for} {Name or other word:} 20 {}]
	    if {$pattern != {} } {
		set result [exec sflwl -l $pattern]
		set arg [exec sflwl $pattern]
		if [regexp {([^ ]+):(.+)$} $arg a arg] {
		    set result [GUI_yesnodialog .askdialog {Connect to} $result]
		    if {$result != {} } {
			set Launcher(target) $arg
			ControlLauncher connect
		    }
		}
	    }
	}
	$m add separator
	$m add command -label "LWL Information..." -command {
		if {[newdialog -path .askdialog -title {LWL Information} -create {
			global lwli env
			if {[array get env SPEAKFREE_ID] == {}} {
				set lwlinfo [list {} {} {} {}]
			} else {
				set lwlinfo [split $env(SPEAKFREE_ID) {:}]
			}
			catch {unset lwli}
			set lwli(name) [lindex $lwlinfo 0]
			set lwli(email) [lindex $lwlinfo 1]
			set lwli(phone) [lindex $lwlinfo 2]
			set lwli(location) [lindex $lwlinfo 3]
			set lwli(hosts) {lwl.fourmilab.ch}
			catch {set lwli(hosts) $env(SPEAKFREE_LWL_TELL)}

			label $w.l -text "Your LWL Information"
			pack $w.l -ipadx 10 -ipady 10 -side top

			frame $w.f2
			frame $w.f2.fl
			frame $w.f2.fr
			
			label $w.f2.fl.l1 -text "Full Name:"
			pack $w.f2.fl.l1 -anchor w
			label $w.f2.fl.l2 -text "Email:"
			pack $w.f2.fl.l2 -anchor w
			label $w.f2.fl.l3 -text "Phone:"
			pack $w.f2.fl.l3 -anchor w
			label $w.f2.fl.l4 -text "Location:"
			pack $w.f2.fl.l4 -anchor w
			label $w.f2.fl.l5 -text "Tell Hosts:"
			pack $w.f2.fl.l5 -anchor w
			
			entry $w.f2.fr.e1 -textvariable lwli(name) -width 40
			pack $w.f2.fr.e1 -anchor w
			entry $w.f2.fr.e2 -textvariable lwli(email) -width 40
			pack $w.f2.fr.e2 -anchor w
			entry $w.f2.fr.e3 -textvariable lwli(phone) -width 40
			pack $w.f2.fr.e3 -anchor w
			entry $w.f2.fr.e4 -textvariable lwli(location) -width 40
			pack $w.f2.fr.e4 -anchor w
			entry $w.f2.fr.e5 -textvariable lwli(hosts) -width 40
			pack $w.f2.fr.e5 -anchor w
			
			pack $w.f2.fl -side left
			pack $w.f2.fr -side right
			pack $w.f2

			button $w.b1 -text "Ok" -command {set GUI_pri_buttonValue 1}
			pack $w.b1 -side bottom -anchor se

		} -variable GUI_pri_buttonValue] == 1} {
			global lwli env

			set env(SPEAKFREE_LWL_TELL) $lwli(hosts)
			set env(SPEAKFREE_ID) "$lwli(name):$lwli(email):$lwli(phone):$lwli(location)"
			if {$env(SPEAKFREE_LWL_TELL) == {}} { catch {unset env(SPEAKFREE_LWL_TELL)} }
			if {$env(SPEAKFREE_ID) == {:::}} { catch {unset env(SPEAKFREE_ID)} }

			ControlLauncher restart
		}
	}
	pack .menubar.lwl -side left

	set value [GetOS]
	if {[GetOS] == "IRIX"} {
		set m .menubar.irix.m
		menubutton .menubar.irix -text "Irix" -underline 0 -menu $m
		menu $m -tearoff 0
		$m add command -label "Audio Panel" -command {
			GUI_print "Opening Audio Panel"
			exec apanel & }
		pack .menubar.irix -side left
	}
	if {[GetOS] == "Linux"} {
		set m .menubar.linux.m
		menubutton .menubar.linux -text "Linux specific" -underline 0 -menu $m
		menu $m -tearoff 0
		$m add command -label "X Mixer" -command {
			GUI_print "Open X Soundcard Mixer"
			exec xmix & }
		pack .menubar.linux -side left
	}

	set m .menubar.help.m
	menubutton .menubar.help -text "Help" -underline 0 -menu $m
	menu $m -tearoff 0
	$m add command -label About -command GUI_help_about
	$m add separator
	$m add command -label {wish Version} -command {
		GUI_print "\n	Tcl version: $tcl_patchLevel"
		GUI_print "	Tk version: $tk_patchLevel\n"
	}
	$m add command -label {Speak Freely Version} -command {
		set id [open_exec -command {sfmike -U} -stdout fd]
		gets $fd line
		gets $fd line
		close_exec $id
		GUI_print "\n	Speak Freely version: [string trim $line]\n"
	}
	$m add separator
	$m add command -label {Speak Freely for Unix} -command {
		GUI_print "\n	Speak Freely for Unix:.\n	Please see the Web Page at:\n\t\thttp://www.fourmilab.ch/speakfree/unix/\n"
	}
	pack .menubar.help -side right -anchor e

	frame .chat
	label .chat.l -text "Chat Input"
	text .chat.v -width 40 -height 1
	button .chat.b0 -text {Send} -command {LauncherChat}
	pack .chat.l -anchor w -side left
	pack .chat.v -fill both -anchor center -side right
	pack .chat.b0 -anchor e -side right
	set GUI_chatinput .char.v

	frame .status
	label .status.l -text "Session Status: "
	listbox .status.v -height 15 -width 70 -yscrollcommand {.status.sy set} -xscrollcommand {.status.sx set} -selectmode browse -exportselection true
	scrollbar .status.sy -command {.status.v yview}
	scrollbar .status.sx -command {.status.v xview} -orient horizontal
	pack .status.l -anchor nw
	pack .status.sy -fill y -expand 0 -anchor e -side right
	pack .status.sx -fill x -expand 0 -anchor s -side bottom
	pack .status.v -fill both -expand 1 -anchor nw -side left
	set GUI_status .status.v

	frame .hosts
	label .hosts.l -text "Host List: "

	listbox .hosts.lt -width 76 -height 1 -font $fixed1
	.hosts.lt insert end [format {  %-30.30s  %-20.20s  %-20.20s} {DNS / IP} {Comment} {Sound file}]
	bindtags .hosts.lt {.hosts.lt}
	proc GUI_hosts_lt_scroll {x j} {
		.hosts.lt xview moveto $x
	}

	set GUI_hostlist_l .hosts.v
	listbox $GUI_hostlist_l -width 76 -height 5 -yscrollcommand {.hosts.sy set} -font $fixed1 -selectmode single -xscrollcommand {GUI_hosts_lt_scroll}
	scrollbar .hosts.sy -command "$GUI_hostlist_l yview"

	foreach {n l} $HostListConstants {
		GUI_hostlist add $n $l
	}
	pack .hosts.l -anchor nw
	pack .hosts.lt -anchor nw
	pack .hosts.sy -fill y -anchor e -side right
	pack .hosts.v -fill both -anchor nw -side left
	#bind .hosts.v <Button-1> {}
	bindtags .hosts.v {. Listbox all}
	set m .hosts.vm
	menu $m -tearoff 0
	$m add command -label "Add host" -command { GUI_connection_new noconnect }
	$m add command -label "Connect to host" -command {
		set h [GUI_hostlist host $GUI_hostlist_l_i]
		if {$h != {} && $h != $Launcher(target)} {
			set Launcher(target) $h;
			$GUI_hostlist_l selection clear 0 end
			$GUI_hostlist_l selection set $GUI_hostlist_l_i
			ControlLauncher connect
		}
	}
	$m add command -label "Close current connection" -command { ControlLauncher disconnect } -state disabled
	$m add separator
	$m add command -label "Send busy" -command {
		set h [GUI_hostlist host $GUI_hostlist_l_i]
		SendSoundFile $h busy
	}
	$m add command -label "Send ring" -command {
		set h [GUI_hostlist host $GUI_hostlist_l_i]
		SendSoundFile $h ring
	}
	$m add command -label "Send sound file..." -command {
		set h [GUI_hostlist host $GUI_hostlist_l_i]
		set file [tk_getOpenFile -title "Select sound file to send to $h"]
		if {$file != {}} {
			SendSoundFile $h $file
		}
	}
	$m add command -label "Stop sound files" -command {
		set h [GUI_hostlist host $GUI_hostlist_l_i]
		foreach x [GUI_hostlist varget $h {sendsnd:exec}] {
			close_exec $x
		}
		GUI_hostlist edit $h -sound {}
		GUI_hostlist varset $h {sendsnd:exec} {}
	}
	$m add separator
	$m add command -label "Edit comment" -command {
		set h [GUI_hostlist host $GUI_hostlist_l_i]
		GUI_hostlist edit $h -comment [GUI_askdialog .askdialog "Comment for $h" "Comment for $h:" 20 [GUI_hostlist get $h -comment]]
	}
	$m add command -label "Remove host" -command {
		if {$Launcher(target) != [GUI_hostlist host $GUI_hostlist_l_i]} {
			$GUI_hostlist_l delete $GUI_hostlist_l_i
		}
	}

	lappend GUI_info(connection:close) $m 2
	lappend GUI_info(hostlist:host) $m 1 $m 4 $m 5 $m 6 $m 9 $m 10
#	lappend GUI_info(hostlist:blank)
	lappend GUI_info(hostlist:sound) $m 7

	bind . <Button-3> {
		if {{%W} == {.hosts.v}} {
			set GUI_hostlist_l_i [%W index @%x,%y]
			set bbox [%W bbox $GUI_hostlist_l_i]
			GUI_set_menu_item_state hostlist sound disabled
			if {[lindex $bbox 1] <= %y && %y <= [expr [lindex $bbox 1] + [lindex $bbox 3]]} {
				GUI_set_menu_item_state hostlist blank disabled
				GUI_set_menu_item_state hostlist host normal
				if {[GUI_hostlist varget [GUI_hostlist host $GUI_hostlist_l_i] {sendsnd:exec}] != {}} {
					GUI_set_menu_item_state hostlist sound normal
				}
			} else {
				GUI_set_menu_item_state hostlist host disabled
				GUI_set_menu_item_state hostlist blank normal
			}
		} else {
			set GUI_hostlist_l_i -1
			GUI_set_menu_item_state hostlist host disabled
			GUI_set_menu_item_state hostlist blank normal
			GUI_set_menu_item_state hostlist sound disabled
		}
		tk_popup .hosts.vm %X %Y
	}

	frame .mike
	frame .mike.buttons
	frame .mike.status
	frame .mike.status2

	label .mike.buttons.l -text "Microphone: "
	radiobutton .mike.buttons.r0 -text "Full Mute" -variable Launcher(:button) -value "Mute" -command { ControlLauncher hangup }
	radiobutton .mike.buttons.r1 -text "Push Button 2 To Talk" -variable Launcher(:button) -value "Push" -command {
		if {$Launcher(exec:id) == {} || $Launcher(pri:ready) != 1} { set Launcher(:button) "Mute" }
		ControlLauncher hangup}
	radiobutton .mike.buttons.r2 -text "Open" -variable Launcher(:button) -value "Open" -command { 
		if {$Launcher(exec:id) == {} || $Launcher(pri:ready) != 1 } { set Launcher(:button) "Mute" }
		ControlLauncher speak }
	pack .mike.buttons.l -side left
	pack .mike.buttons.r0 -side left
	pack .mike.buttons.r1 -side left
	pack .mike.buttons.r2 -side left
	bind . <ButtonPress-2> { if {$Launcher(:button) == "Push"} { ControlLauncher speak } }
	bind . <ButtonRelease-2> { if {$Launcher(:button) == "Push"} { ControlLauncher hangup } }

	label .mike.status.l -text "Microphone Status:"
	label .mike.status.ms -textvariable Launcher(:state:text) -font $fixed1 -width 10
	pack .mike.status.l -side left
	pack .mike.status.ms -side left

	label .mike.status.l2 -text "Microphone Transmitting To: "
	label .mike.status.v2 -textvariable Launcher(target) -font $fixed1
	pack .mike.status.l2 -side left
	pack .mike.status.v2 -side left

	pack .mike.buttons -side top -anchor w
	pack .mike.status -side bottom -anchor w

	pack .mike -anchor nw
	pack .hosts -anchor nw
	pack .chat -anchor nw
	pack .status -fill both -expand 1
}

proc GUI_askdialog {w title text width value} {
    return [newdialog -path $w -title $title -create {
	upvar width width value value text text
	frame $w.bot
	frame $w.but
	frame $w.top
	$w.bot configure -relief raised -bd 1
	$w.top configure -relief raised -bd 1
	pack $w.top -fill both -expand 1
	pack $w.bot -fill both
	pack $w.but -fill both

	option add *Dialog.msg.wrapLength 3i widgetDefault
	label $w.msg -justify left -text $text
	pack $w.msg -in $w.top -side right -expand 1 -fill both -padx 3m -pady 3m

	text $w.bot.in -width $width -height 1
	$w.bot.in insert insert $value
	pack $w.bot.in -expand 1 -fill x

	button $w.but.button0 -text {Ok} -command {set GUI_pri_askdialog_waitvar 0}
	button $w.but.button1 -text {Cancel} -command {set GUI_pri_askdialog_waitvar 1}

	pack $w.but.button1 -side right
	pack $w.but.button0 -side left

	bind $w <Return> "
		$w.but.button0 configure -state active -relief sunken
		update idletasks
		set GUI_pri_askdialog_waitvar 0
	"
	bind $w.bot.in <Return> "
		$w.but.button0 configure -state active -relief sunken
		update idletasks
		after 100
		set askdialog_waitvar 0
	"} -variable GUI_pri_askdialog_waitvar -after {
		if {$GUI_pri_askdialog_waitvar == 0} {
			catch {set GUI_pri_askdialog_waitvar [string trim [$w.bot.in get 1.0 end]]}
		} else {
			set GUI_pri_askdialog_waitvar $value
		}
	}]
}

proc GUI_yesnodialog {w title text} {
    return [newdialog -path $w -title $title -create {
	upvar width width value value text text
	frame $w.bot
	frame $w.but
	frame $w.top
	$w.bot configure -relief raised -bd 1
	$w.top configure -relief raised -bd 1
	pack $w.top -fill both -expand 1
	pack $w.bot -fill both
	pack $w.but -fill both

	option add *Dialog.msg.wrapLength 3i widgetDefault
	label $w.msg -justify left -text $text
	pack $w.msg -in $w.top -side right -expand 1 -fill both -padx 3m -pady 3m

	button $w.but.button0 -text {Yes} -command {set GUI_pri_askdialog_waitvar 0}
	button $w.but.button1 -text {No} -command {set GUI_pri_askdialog_waitvar 1}

	pack $w.but.button1 -side right
	pack $w.but.button0 -side left

	bind $w <Return> "
		$w.but.button0 configure -state active -relief sunken
		update idletasks
		set GUI_pri_askdialog_waitvar 0
	"} -variable GUI_pri_askdialog_waitvar -after {
		if {$GUI_pri_askdialog_waitvar == 0} {
		    catch {set GUI_pri_askdialog_waitvar {yes} }
		} else {
		    set GUI_pri_askdialog_waitvar {}
		}
	}]
}

proc GUI_help_about {} {
	global TkSpeakFreeVersion

	GUI_print "\n	TkSpeakFree version $TkSpeakFreeVersion"
	GUI_print "	Author: Martin Goellnitz, <martin@goellnitz.de>"
	GUI_print "	Mostly based on work by Shawn Pearce, <spearce@injersey.com>\n"
}
