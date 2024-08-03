# newdialog.tcl --
#
# newdialog
#
# This procedure displays a dialog box, waits for a button in the dialog
# to be invoked, then returns the index of the selected button.  If the
# dialog somehow gets destroyed, -1 is returned.
#
# Arguments:
#	-path		-- path to use for window, default=.newdialog
#	-title		-- title to use for window, default=Dialog
#	-create		-- script to pack dialog
#	-after		-- script to run after dialog has been closed, either through a destroy or a change of variable
#	-variable	-- variable to watch for change
#	-grab		-- boolean whether to take focus or not

proc newdialog {args} {
	set w .newdialog
	set title Dialog
	set create {}
	set after {}
	set var {}
	set grabFocus true 
	foreach {n v} $args {
		switch -exact -- $n {
			-path {set w $v}
			-title {set title $v}
			-create {set create $v}
			-after {set after $v}
			-variable {set var $v}
			-grab {set grabFocus $v}
		}
	}
	if {$var == {}} {
		error {-variable cannot be null, must be a global}
		return
	}
	global $var

	catch {destroy $w}
	toplevel $w
	wm title $w $title
	wm iconname $w $title
	bind $w <Destroy> "set $var {<Destroyed>}"
	wm protocol $w WM_DELETE_WINDOW "destroy $w"

	# The following command means that the dialog won't be posted if
	# [winfo parent $w] is iconified, but it's really needed;  otherwise
	# the dialog can become obscured by other windows in the application,
	# even though its grab keeps the rest of the application from being used.

	wm transient $w [winfo toplevel [winfo parent $w]]

	eval $create

	wm withdraw $w
	update idletasks
	set wparent [winfo parent $w]
	set x [expr [winfo width $wparent]/2 - [winfo reqwidth $w]/2 + [winfo x $wparent]]
	set y [expr [winfo height $wparent]/2 - [winfo reqheight $w]/2 + [winfo y $wparent]]

	wm geom $w +$x+$y
	wm deiconify $w

	if {$grabFocus} {
		# Set a grab and claim the focus too.
		set oldFocus [focus]
		set oldGrab [grab current $w]
		if {$oldGrab != ""} {
			set grabStatus [grab status $oldGrab]
		}
		grab $w
	}

	tkwait variable $var
	eval $after

	if {$grabFocus} {
		catch {focus $oldFocus}
	}
	catch {
		# It's possible that the window has already been destroyed,
		# hence this "catch".  Delete the Destroy handler so that
		# $var doesn't get reset by it.

		bind $w <Destroy> {}
		destroy $w
	}
	if {$grabFocus} {
		if {$oldGrab != ""} {
			if {$grabStatus == "global"} {
			    grab -global $oldGrab
			} else {
			    grab $oldGrab
			}
		}
	}

	return [eval "return $$var"]
}

# newwindow
# Arguments
#	-path	-- path to use for window
#	-title	-- title to use for window
#	-delete	-- script to run when window is deleted by window manager

proc newwindow {args} {

	set w {}
	set title {}
	set del {}

	foreach {n v} $args {
		switch -exact -- $n {
			-path {set w $v}
			-title {set title $v}
			-delete {set del $v}
		}
	}
	if {$w == {}} {
		error {-path cannot be null}
		return
	}

	catch {destroy $w}
	toplevel $w
	wm title $w $title
	wm iconname $w $title
	if {$del != {}} {
		wm protocol $w WM_DELETE_WINDOW $del
	}
	return $w
}
