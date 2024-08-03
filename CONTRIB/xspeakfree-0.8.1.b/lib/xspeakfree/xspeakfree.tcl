# xspeakfree
#
# Author: Shawn Pearce
# Email: spearce@injersey.com
#
# xspeakfree is a free Tcl/Tk (wish) frontend to Speak Freely
#
# For more information about Speak Freely see:
#	http://www.fourmilab.ch/
#
# For more information about Tcl/Tk see:
#	http://sunscript.sun.com/TclTkCore/
#
# Please note I am not the author of Speak Freely, nor the
# author of wish, only the author of this frontend.
#
# Please send all comments, complaints and suggestions to me at the
# above email address.

set LibDir [file dirname $argv0]

source [file join $LibDir xspeakfree-version.tcl]
source [file join $LibDir open3.tcl]
source [file join $LibDir newdialog.tcl]
source [file join $LibDir gui.tcl]
source [file join $LibDir cmd.tcl]
