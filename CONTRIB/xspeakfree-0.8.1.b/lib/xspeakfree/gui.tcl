proc GUI_print {args} {
	global GUI_status
	
	regsub -all "\t" [join $args] {    } x

	foreach i [split $x "\n"] {
		$GUI_status insert end $i
	}
	$GUI_status see end
}

proc GUI_connection_new { {noconnect {}} } {
	global Mike GUI_hostlist_l

	if {$noconnect == {}} {
		set host [GUI_askdialog .askdialog {Send Audio to Remote Host} {Send Audio to Remote Host:} 20 {}]
	} else {
		set host [GUI_askdialog .askdialog {Add Host} {Add Host:} 20 {}]
	}
	set host [string trim $host]

	if {$host != {} && $host != $Mike(target)} {
		GUI_hostlist add $host
		if {$noconnect == {}} {
			$GUI_hostlist_l selection clear 0 end
			$GUI_hostlist_l selection set end
			set Mike(target) $host
			ControlMike open
		}
	}
}

proc GUI_hostlist {cmd {host {}} args} {
	global GUI_hostlist_l Mike HostListConstants GUI_hostlist_values
	
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

	set label [format {%-1.1s %-30.30s  %-30.30s  %-10.10s} $xmit $host $comment $sound]

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
			if {$Mike(hostlist:add:send_busy) == 1 && $Mike(target) != {}} {
				SendSoundFile $host busy
			} else {
				if {$host != {localhost} && $Mike(hostlist:add:play_ring) == 1} {
					SendSoundFile localhost ring noprint
				}
			}
		}
	}

	if {$cmd == {remove}} {
		if {$Mike(target) == $host} { return }

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

	wm title . xspeakfree
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
		ControlMike close
		$GUI_hostlist_l selection clear 0 end	
		} -state disabled
	$m add command -label "Restart sfspeaker" -command { ControlSpeaker open }
	$m add separator
	$m add command -label "Play audio file..." -command {
		set file [tk_getOpenFile]
		if {$file != {}} {
			GUI_hostlist add localhost -connection false -transmit false
			SendSoundFile localhost $file
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
	$m add command -label "Local Port..." -command {
		set lport [GUI_askdialog .askdialog {Local Port} {Local Port:} 10 $Speaker(port)]
		if {$lport != $Speaker(port)} {
			set Speaker(port) $lport
			ControlSpeaker restart
		}
	}
	$m add command -label "Encryption Options..." -command {
		if {[newdialog -path .askdialog -title {Encryption Options} -create {
			global Speaker

			label $w.l -text "Encryption Options"
			pack $w.l -ipadx 10 -ipady 10 -side top

			frame $w.f1
			label $w.f1.l -text "Encryption Method:"
			radiobutton $w.f1.r0 -text "None" -value { } -variable Speaker(:crypt:method)
			radiobutton $w.f1.r1 -text "Blowfish" -value {-BF} -variable Speaker(:crypt:method)
			radiobutton $w.f1.r2 -text "IDEA" -value {-I} -variable Speaker(:crypt:method)
			radiobutton $w.f1.r3 -text "DES" -value {-K} -variable Speaker(:crypt:method)
			radiobutton $w.f1.r4 -text "File" -value {-O} -variable Speaker(:crypt:method)
			radiobutton $w.f1.r5 -text "PGP" -value {-Z} -variable Speaker(:crypt:method)
			pack $w.f1.l -side left -anchor w
			pack $w.f1.r0 -side left -anchor w
			pack $w.f1.r1 -side left -anchor w
			pack $w.f1.r2 -side left -anchor w
			pack $w.f1.r3 -side left -anchor w
			pack $w.f1.r4 -side left -anchor w
			pack $w.f1.r5 -side left -anchor w
			pack $w.f1 -anchor w
			
			frame $w.f2
			frame $w.f2.fl
			frame $w.f2.fr
			
			label $w.f2.fl.l1 -text "Blowfish Key:"
			pack $w.f2.fl.l1 -anchor w
			label $w.f2.fl.l2 -text "IDEA Key:"
			pack $w.f2.fl.l2 -anchor w
			label $w.f2.fl.l3 -text "DES Key:"
			pack $w.f2.fl.l3 -anchor w
			label $w.f2.fl.l4 -text "File:"
			pack $w.f2.fl.l4 -anchor w
			label $w.f2.fl.l5 -text "PGP Pass Phrase:"
			pack $w.f2.fl.l5 -anchor w
			
			entry $w.f2.fr.e1 -textvariable Speaker(:crypt:-BF) -width 40
			pack $w.f2.fr.e1 -anchor w
			entry $w.f2.fr.e2 -textvariable Speaker(:crypt:-I) -width 40
			pack $w.f2.fr.e2 -anchor w
			entry $w.f2.fr.e3 -textvariable Speaker(:crypt:-K) -width 40
			pack $w.f2.fr.e3 -anchor w
			frame $w.f2.fr.f1
			entry $w.f2.fr.f1.e4 -textvariable Speaker(:crypt:-O) -width 25
			button $w.f2.fr.f1.b1 -text "Select File" -command {
					set f [ tk_getOpenFile -title {Crypt file:} -initialfile [file tail $Speaker(:crypt:-O)] -initialdir [file dirname $Speaker(:crypt:-O)] ]
					if {$f != {}} {
						set Speaker(:crypt:-O) $f
					}
				}
			pack $w.f2.fr.f1.e4 -side left -anchor w
			pack $w.f2.fr.f1.b1 -side left -anchor w
			pack $w.f2.fr.f1 -anchor w
			entry $w.f2.fr.e5 -textvariable Speaker(:crypt:-Z) -width 40
			pack $w.f2.fr.e5 -anchor w
			
			pack $w.f2.fl -side left
			pack $w.f2.fr -side right
			pack $w.f2

			button $w.b1 -text "Ok" -command {set GUI_pri_buttonValue 1}
			pack $w.b1 -side bottom -anchor se

		} -variable GUI_pri_buttonValue] == 1} {
			ModifyArgs Speaker
			ModifyArgs Mike
		}
	}
	$m add cascade -label "Jitter Compensation" -menu .menubar.options.m.jitter
	$m add checkbutton -label "Disable Remote Ring Requests" -command {ModifyArgs Speaker} -onvalue {-n} -offvalue { } -variable Speaker(args:disable_remote_ring)
	$m add command -label "Record Audio To File ..." -command {
		if {[newdialog -path .askdialog -title {Record Audio To File} -create {
				global Speaker

				label $w.l -text "Record Audio To File"
				pack $w.l -ipadx 10 -ipady 10 -side top
				
				frame $w.f3
				label $w.f3.l -text "Record Audio:"
				radiobutton $w.f3.r1 -text "Yes" -value 1 -variable Speaker(:record:onoff) -command "
					$w.f2.r1 configure -state normal
					$w.f2.r2 configure -state normal
					$w.f.b configure -state normal
					$w.f.v configure -fg black
					$w.f.l configure -fg black
				"
				radiobutton $w.f3.r2 -text "No" -value 0 -variable Speaker(:record:onoff) -command "
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
				label $w.f.v -textvariable Speaker(:record:file)
				pack $w.f.v -side left -anchor w

				button $w.f.b -text "Select File" -command {
					set f [ tk_getSaveFile -title {Record into:} -initialfile [file tail $Speaker(:record:file)] -initialdir [file dirname $Speaker(:record:file)] ]
					if {$f != {}} {
						set Speaker(:record:file) $f
					}
				}
				pack $w.f.b -side right -anchor w
				pack $w.f

				frame $w.f2
				radiobutton $w.f2.r1 -text "Overwrite file" -value {} -variable Speaker(:record:append)
				radiobutton $w.f2.r2 -text "Append to file" -value {+} -variable Speaker(:record:append)
				pack $w.f2.r1 -side right -anchor w
				pack $w.f2.r2 -side right -anchor w
				pack $w.f2

				frame $w.fp
				pack $w.fp -ipady 20

				if {$Speaker(:record:onoff) == 0} {
					$w.f2.r1 configure -state disabled
					$w.f2.r2 configure -state disabled
					$w.f.b configure -state disabled
					$w.f.v configure -fg grey
					$w.f.l configure -fg grey
				}

				
				frame $w.fb
				button $w.fb.b1 -text "Ok" -command { 
					if {$Speaker(:record:file) == {}} {
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
			ModifyArgs Speaker
		}
	}
	$m add command -label "Additional sfspeaker Arguments..." -command {
		set f [GUI_askdialog .askdialog {sfspeaker arguments} {Additional sfspeaker arguments} 20 $Speaker(args:user)]
		if {$f != $Speaker(args:user)} {
			set Speaker(args:user) $f
			ModifyArgs Speaker
		}
	}
	$m add separator
	$m add cascade -label "Compression" -menu .menubar.options.m.compression
	$m add cascade -label "Transmission Protocol" -menu .menubar.options.m.protocol
	$m add checkbutton -label "Send Remote Ring" -command {ModifyArgs Mike} -onvalue {-R} -offvalue { } -variable Mike(args:ring)
	$m add command -label "Additional sfmike Arguments..." -command {
		set f [GUI_askdialog .askdialog {sfmike arguments} {Additional sfmike arguments} 20 $Mike(args:user)]
		if {$f != $Mike(args:user)} {
			set Mike(args:user) $f
			ModifyArgs Mike
		}
	}
	$m add separator
	$m add checkbutton -label "Send Ring Sound To Targets" -onvalue 1 -offvalue 0 -variable Mike(connect:send_ring)
	$m add separator
	$m add checkbutton -label "Sound Ring For New Connections" -onvalue 1 -offvalue 0 -variable Mike(hostlist:add:play_ring)
	$m add checkbutton -label "Send Busy Signal If Already Connected" -onvalue 1 -offvalue 0 -variable Mike(hostlist:add:send_busy)
	$m add separator
	$m add command -label "Look Who's Listening Information..." -command {
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

			label $w.l -text "LWL Information"
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

			ControlSpeaker restart
		}
	}
	$m add command -label "Search Look Who's Listening..." -command {
		set pat [GUI_askdialog .askdialog "Search Look Who's Listening" "Pattern:" 80 {}]
		set id [open_exec -stdout out -stderr err -command "sflwl -d $pat"]
	
		set info(NAME) {}

		while {[gets $err line] > -1} {
			if {[regexp {IP = ([^ ]+) Port ([^ ]+)} $line a ip port]} {
				set info(HOST) "$ip:$port"
			} else {
				regexp {([^ ]+) = (.+)$} $line a name val
				set info($name) $val
			}
		}
		close_exec $id

		if {$info(NAME) == {}} {
			GUI_print "[timestr]: ERROR: No user matched the pattern '$pat' in LWL information at $env(SPEAKFREE_LWL_TELL)."
		} else {
			GUI_hostlist add $info(HOST) -comment "$info(NAME) @ $info(LOC)" 
			GUI_print "[timestr]: LWL Results:"
			GUI_print [format {%-10.10s%-30.30s %-30.30s} {} $info(NAME) $info(EMAIL)]
			GUI_print [format {%-10.10s%-30.30s %-30.30s} {} $info(PHONE) $info(LOC)]
			GUI_print [format {%-10.10s%-30.30s %-30.30s} {} $info(CNAME) $info(TOOL)]
		}
	}
	pack .menubar.options -side left

	set m .menubar.options.m.compression
	menu $m -tearoff 0
	$m add checkbutton -label "Simple Compression" -command {ModifyArgs Mike} -onvalue {-C} -offvalue { } -variable Mike(args:simplecompression)
	$m add radiobutton -label "No Compression" -command {ModifyArgs Mike} -value {-N} -variable Mike(args:compression)
	$m add radiobutton -label "GSM Compression" -command {ModifyArgs Mike} -value {-T} -variable Mike(args:compression)
	$m add radiobutton -label "ADPCM Compression" -command {ModifyArgs Mike} -value {-F} -variable Mike(args:compression)
	$m add radiobutton -label "LPC Compression" -command {ModifyArgs Mike} -value {-LPC} -variable Mike(args:compression)
	$m add radiobutton -label "LPC-10 1x Compression" -command {ModifyArgs Mike} -value {-LPC10R1} -variable Mike(args:compression)
	$m add radiobutton -label "LPC-10 2x Compression" -command {ModifyArgs Mike} -value {-LPC10R2} -variable Mike(args:compression)
	$m add radiobutton -label "LPC-10 3x Compression" -command {ModifyArgs Mike} -value {-LPC10R3} -variable Mike(args:compression)
	$m add radiobutton -label "LPC-10 4x Compression" -command {ModifyArgs Mike} -value {-LPC10R4} -variable Mike(args:compression)

	set m .menubar.options.m.jitter
	menu $m -tearoff 0
	$m add radiobutton -label "None" -command {ModifyArgs Speaker} -value " " -variable Speaker(args:jitter)
	$m add separator
	$m add radiobutton -label "1/10 Second" -command {ModifyArgs Speaker} -value "-J100" -variable Speaker(args:jitter)
	$m add radiobutton -label "1/4 Second" -command {ModifyArgs Speaker} -value "-J250" -variable Speaker(args:jitter)
	$m add radiobutton -label "1/2 Second" -command {ModifyArgs Speaker} -value "-J500" -variable Speaker(args:jitter)
	$m add radiobutton -label "3/4 Second" -command {ModifyArgs Speaker} -value "-J750" -variable Speaker(args:jitter)
	$m add radiobutton -label "1 Second" -command {ModifyArgs Speaker} -value "-J1000" -variable Speaker(args:jitter)
	$m add radiobutton -label "2 Seconds" -command {ModifyArgs Speaker} -value "-J2000" -variable Speaker(args:jitter)
	$m add radiobutton -label "3 Seconds" -command {ModifyArgs Speaker} -value "-J3000" -variable Speaker(args:jitter)

	set m .menubar.options.m.protocol
	menu $m -tearoff 0
	$m add radiobutton -label "Speak Freely" -command {ModifyArgs Mike} -value " " -variable Mike(args:protocol)
	$m add radiobutton -label "Real Time Protocol (RTP)" -command {ModifyArgs Mike} -value "-RTP" -variable Mike(args:protocol)
	$m add radiobutton -label "Visual Audio Tool (VAT)" -command {ModifyArgs Mike} -value "-VAT" -variable Mike(args:protocol)

	if {[GetOS] == "IRIX"} {
		set m .menubar.irix.m
		menubutton .menubar.irix -text "Irix" -underline 0 -menu $m
		menu $m -tearoff 0
		$m add command -label "Audio Panel" -command {
			GUI_print "Opening Audio Panel"
			exec apanel & }
		pack .menubar.irix -side left
	}

	set m .menubar.help.m
	menubutton .menubar.help -text "Help" -underline 0 -menu $m
	menu $m -tearoff 0
	$m add command -label About -command GUI_help_about
	$m add separator
	$m add command -label {wish Version} -command {
		GUI_print "---	Tcl version: $tcl_patchLevel"
		GUI_print "---	Tk version: $tk_patchLevel"
	}
	$m add command -label {Speak Freely Version} -command {
		set id [open_exec -command {sfmike -U} -stdout fd]
		gets $fd line
		gets $fd line
		close_exec $id
		GUI_print "---	Speak Freely version: [string trim $line]"
	}
	$m add separator
	$m add command -label {Speak Freely for Unix} -command {
		GUI_print "---	Speak Freely for Unix: http://www.fourmilab.ch/speakfree/unix/"
	}
	pack .menubar.help -side right -anchor e

	frame .status
	label .status.l -text "Session Status: "
	listbox .status.v -height 7 -width 60 -yscrollcommand {.status.sy set} -xscrollcommand {.status.sx set} -selectmode browse -exportselection true
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
	.hosts.lt insert end [format {  %-30.30s  %-30.30s  %-10.10s} {DNS / IP} {Comment} {Sound file}]
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
		if {$h != {} && $h != $Mike(target)} {
			set Mike(target) $h;
			$GUI_hostlist_l selection clear 0 end
			$GUI_hostlist_l selection set $GUI_hostlist_l_i
			ControlMike open
		}
	}
	$m add command -label "Close current connection" -command { ControlMike close } -state disabled
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
		if {$Mike(target) != [GUI_hostlist host $GUI_hostlist_l_i]} {
			$GUI_hostlist_l delete $GUI_hostlist_l_i
		}
	}

	lappend GUI_info(connection:close) $m 2
	lappend GUI_info(hostlist:host) $m 1 $m 4 $m 5 $m 6 $m 9 $m 10
	lappend GUI_info(hostlist:sound) $m 7

	bind . <Button-3> {
		if {{%W} == {.hosts.v}} {
			set GUI_hostlist_l_i [%W index @%x,%y]
			set bbox [%W bbox $GUI_hostlist_l_i]
			GUI_set_menu_item_state hostlist sound disabled
			if {$bbox != "" && [lindex $bbox 1] <= %y && %y <= [expr [lindex $bbox 1] + [lindex $bbox 3]]} {
				GUI_set_menu_item_state hostlist host normal
				if {[GUI_hostlist varget [GUI_hostlist host $GUI_hostlist_l_i] {sendsnd:exec}] != {}} {
					GUI_set_menu_item_state hostlist sound normal
				}
			} else {
				GUI_set_menu_item_state hostlist host disabled
			}
		} else {
			set GUI_hostlist_l_i -1
			GUI_set_menu_item_state hostlist host disabled
			GUI_set_menu_item_state hostlist sound disabled
		}
		tk_popup .hosts.vm %X %Y
	}

	frame .localport
	label .localport.l -text "Local Port: "
	label .localport.v -textvariable Speaker(port) -font $fixed1
	pack .localport.l -side left
	pack .localport.v

	frame .mike
	frame .mike.buttons
	frame .mike.status
	frame .mike.status2

	label .mike.buttons.l -text "Microphone: "
	radiobutton .mike.buttons.r0 -text "Full Mute" -variable Mike(:button) -value "Mute" -command { ControlMike off }
	radiobutton .mike.buttons.r1 -text "Push Button 2 To Talk" -variable Mike(:button) -value "Push" -command {
		if {$Mike(exec:id) == {} || $Mike(pri:ready) != 1} { set Mike(:button) "Mute" }
		ControlMike off}
	radiobutton .mike.buttons.r2 -text "Open" -variable Mike(:button) -value "Open" -command { 
		if {$Mike(exec:id) == {} || $Mike(pri:ready) != 1 } { set Mike(:button) "Mute" }
		ControlMike on }
	pack .mike.buttons.l -side left
	pack .mike.buttons.r0 -side left
	pack .mike.buttons.r1 -side left
	pack .mike.buttons.r2 -side left
	bind . <ButtonPress-2> { if {$Mike(:button) == "Push"} { ControlMike on } }
	bind . <ButtonRelease-2> { if {$Mike(:button) == "Push"} { ControlMike off } }

	label .mike.status.l -text "Microphone Status:"
	label .mike.status.ms -textvariable Mike(:state:text) -font $fixed1 -width 10
	pack .mike.status.l -side left
	pack .mike.status.ms -side left

	label .mike.status.l2 -text "Microphone Transmitting To: "
	label .mike.status.v2 -textvariable Mike(target) -font $fixed1
	pack .mike.status.l2 -side left
	pack .mike.status.v2 -side left

	pack .mike.buttons -side top -anchor w
	pack .mike.status -side bottom -anchor w

	pack .localport -anchor nw
	pack .mike -anchor nw
	pack .hosts -anchor nw
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

	pack $w.but.button1 -side left
	pack $w.but.button0 -side right

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

proc GUI_help_about {} {
	global XSpeakFreeVersion

	GUI_print "	xspeakfree version $XSpeakFreeVersion"
	GUI_print "	Author:   Shawn Pearce, <spearce@spearce.org>"
	GUI_print "	Website:  http://www.spearce.org/projects/xspeakfree\n"
}
