# Shawn Pearce, <spearce@injersey.com>, 1997
# similiar to perl's open2 and open3 comamnds

rename exit tcl_core:exit

set EXEC_cnt 0

proc open_exec {args} {
	global EXEC EXEC_cnt
	
	incr EXEC_cnt

	foreach {n v} $args {
		switch -exact -- $n {
			-command { 
				set cmd {}
				foreach x [string trim $v] {
					if {$x != {} && $x != { }} { lappend cmd $x }
				}
			}
			-stdin { upvar 1 $v stdin; set stdin {} }
			-stdout { upvar 1 $v stdout; set stdout {} }
			-stderr { upvar 1 $v stderr; set stderr {} }
		}
	}

	set path "/tmp/tclsh-exec.[pid].$EXEC_cnt"
	set EXEC(exec$EXEC_cnt:fd) {}

	if [info exists stdin] {
		exec mkfifo "$path.0"
		lappend tmpfd [open "$path.0" r+]
		set stdin [open "$path.0" w]
		
		append cmd " <$path.0 "
		lappend EXEC(exec$EXEC_cnt:fd) $stdin
	}
	if [info exists stdout] {
		exec mkfifo "$path.1"
		lappend tmpfd [open "$path.1" r+]
		set stdout [open "$path.1" r]

		lappend EXEC(exec$EXEC_cnt:fd) $stdout
		
		if [info exists stderr] {
			append cmd " >$path.1 "
		} else {
			append cmd " >&$path.1 "
		}
	}
	if [info exists stderr] {
		exec mkfifo "$path.2"
		lappend tmpfd [open "$path.2" r+]
		set stderr [open "$path.2" r]

		lappend EXEC(exec$EXEC_cnt:fd) $stderr
			
		append cmd " 2>$path.2 "
	}
	
	set EXEC(exec$EXEC_cnt:cmd) $cmd
	set EXEC(exec$EXEC_cnt:path) $path
	set EXEC(exec$EXEC_cnt:pid) [eval "exec $cmd &"]
	
	foreach tempfd $tmpfd {
		close $tempfd
	}

	set tempfd [open "$path.pid" w]
	puts $tempfd $EXEC(exec$EXEC_cnt:pid)
	close $tempfd

	return "exec$EXEC_cnt"
}

proc close_exec {id} {
	global EXEC
	
	catch {exec kill $EXEC($id:pid)}
	catch {close [lindex $EXEC($id:fd) 0]}; catch {file delete -force "$EXEC($id:path).0"}
	catch {close [lindex $EXEC($id:fd) 1]}; catch {file delete -force "$EXEC($id:path).1"}
	catch {close [lindex $EXEC($id:fd) 2]}; catch {file delete -force "$EXEC($id:path).2"}
	catch {file delete -force "$EXEC($id:path).pid"}

	catch {unset EXEC($id:pid)}
	catch {unset EXEC($id:path)}
	catch {unset EXEC($id:cmd)}
	catch {unset EXEC($id:fd)}
}

proc exec_closeall {} {
	global EXEC
	
	foreach {n v} [array get EXEC "*:path"] {
		regexp {^(exec[0-9]+):} $n a id
		close_exec $id
	}
}

proc exit { {status 0} } {
	exec_closeall
	tcl_core:exit $status
}
