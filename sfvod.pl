#
#               Speak Freely Voice on Demand Server
#

    $host_timeout = 30;
    $live = 0;
    $lchild = -1;
    $lwltell = -1;
    $log = 0;
    $verbose = 0;
    $hexdump = 0;
    $debug = 0;
    $port = 3456;
    $soundfile = "";
    $moptions = "";
    $program = "sfmike -a";

    @proto = ( "-vat ", "", "-rtp ", "" );
    @protoName = ( "VAT", "Speak_Freely", "RTP", "Gibberish" );
    @mname = ( "Jan", "Feb", "Mar", "Apr", "May", "Jun",
               "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" );

    $me = $0;
    if (rindex($me, "/") >= 0) {
        $me = substr($me, rindex($me, "/") + 1);
    }

    #   Process command line arguments

    $arghhh = 1;
    while (@ARGV) {
        $arg = shift;
        if (substr($arg, 0, 1) eq "-" & $arghhh) {

            #   An argument of a single dash terminates our processing
            #   of arguments.  Any that remain are passed to sfmike.

            if (length($arg) == 1) {
                $arghhh = 0;
                next;
            }
            $opt = substr($arg, 1, 1);
            $opt =~ tr/A-Z/a-z/;
            $opa = substr($arg, 2);
            if ($opt eq 'a') {        # -A  --  Live audio mode
                $live = 1;
            } elsif ($opt eq 'd') {   # -D  --  Debug output
                $debug = 1;
            } elsif ($opt eq 'l') {   # -Lfile  --  Log requests in file
                $log = 1;
                open(LOGFILE, ">>" . $opa);
                select(LOGFILE);
                $| = 1;
                select(stdout);
            } elsif ($opt eq 'p') {   # -Pport  --  Listen on given port
                $port = $opa;
            } elsif ($opt eq 'r') {   # -Rprog  --  Run "prog" to serve requests
                $program = $opa;
            } elsif ($opt eq 't') {   # -Ttime  --  Time out hosts after time seconds
                $host_timeout = $opa;
                if ($host_timeout < 20) {
                    print "Timeout (-t) must be at least 20 seconds.\n";
                    exit;
                }
            } elsif ($opt eq 'u' || $opt eq '?') {
                print "sfvod  --  Speak Freely voice on demand server.\n";
                if (defined $version) {
                    print "           $version.\n"; 
                }
                print "Usage: sfvod [options] soundfile...\n";
                print "Options:\n";
                print "    -A         Send live audio\n";
                print "    -Lfile     Log requests in file\n";
                print "    -Pport     Listen on given port (default 3456)\n";
                print "    -Rprog     Run prog to process request (default sfmike)\n";
                print "    -Ttime     Time out inactive hosts after time seconds\n";
                print "    -U         Print this message\n";
                print "    -V         Show host connects and disconnects\n";
                print "    -X         Dump host addresses and packets in hex\n";
                print "    -          Pass subsequent options to sfmike\n";
                exit;
            } elsif ($opt eq "v") {   # -V  --  Verbose output
                $verbose = 1;
            } elsif ($opt eq "x") {   # -X  --  Hexadecimal dump
                $hexdump = 1;
            }
        } else {
            if (substr($arg, 0, 1) eq "-") {
                if (length($moptions) > 0) {
                    $moptions .= " ";
                }
                $moptions .= $arg;
            } else {
                if (length($soundfile) > 0) {
                    $soundfile .= " ";
                }
                $soundfile .= $arg;
            }
        }
    }

#   $AF_INET = 2;                     # These can vary from system to
#   $SOCK_DGRAM = 2;                  # system, so they're suppled by the Makefile
    $EINTR = 4;                       # Interrupted system call status
    $ECHILD = 10;                     # No children status
    $sockaddr = 'S n a4 x8';
    $protocol = getprotobyname('udp'); # We use UDP protocol
    $WNOHANG = defined &WNOHANG ? &WNOHANG : 1;
    $SIG{'CHLD'} = 'reaper';          # Register child process reaper

    if ($verbose) {
        print "$me: listening on port $port.\n";
    }

    #   Create a socket to listen on the control port and bind
    #   it to the port number.

    $sock = pack($sockaddr, $AF_INET, $port + 1, "\0\0\0\0");
    socket(S, $AF_INET, $SOCK_DGRAM, $protocol) || die "Error creating socket: $!";
    bind(S, $sock) || die "Error binding socket: $!";
    select(S);
    $| = 1;
    select(stdout);

    $SIG{'ALRM'} = 'tick';            # Register timeout handler
    alarm(10);                        # Set timeout handler

    #   If SPEAKFREE_LWL_TELL is defined, fork a process to publish
    #   our identity on the LWL server.

    if (defined($ENV{'SPEAKFREE_LWL_TELL'})) {
        if (($lwltell = fork()) == 0) {
            $SIG{'INT'} = 'killed';
            $zexec = "sfspeaker -w$port";
            if ($debug) {
                print("Exec: $zexec\n");
            }
            exec($zexec);
            exit;
        }
    }

    $con = 1;
    while (1) {

        #   Wait until a packet arrives from the control port.

        #   You might be wondering why we're doing a select()
        #   here when we're only interested in waiting on a
        #   single file discriptor.  Well, the reason is that
        #   there's a stone bug in Perl 5.004 which causes the
        #   first recv() after a signal was processed (hence using
        #   the "restartable system call" mechanism) to return
        #   the null string as the sender's address, notwithstanding
        #   the fact that the data for the packet has been correcly
        #   stored into the string argument.
        #
        #   If one uses select(), however, to block until a
        #   packet is ready to recv(), the problem does not
        #   occur.  So that's the way we'll do it.

        $rin = '';
        vec($rin, fileno(S), 1) = 1;
        $nfound = select($rout = $rin, undef, undef, undef);

        if ($nfound == 0) {
#           &tick();
            next;
        }

        $addr = recv(S, $sockread, 512, 0);
        if (!defined($addr)) {
            if ($debug) {
                print("Recv error: $!\n");
            }
            if ($! == $EINTR || $! == $ECHILD) {
                if ($debug) {
                    print(" ...ignoring\n");
                }
                next;
            }
            die "Error receiving from socket: $!";
        }
        if ($hexdump) {
            printf("Address, length %d:\n", length($addr));
            &hexdump($addr, '    ');
        }
        if (length($addr) < 16) {
            if ($debug) {
                print("Recv: Void address\n");
            }
            next;
        }
        if ($hexdump) {
            printf("Packet, length %d:\n", length($sockread));
            &hexdump($sockread, '    ');
        }
        $pr = (ord($sockread) >> 6) & 3;  # Extract protocol from first byte
        ($af, $rport, $inetaddr) = unpack($sockaddr, $addr);
        @inetaddr = unpack('C4', $inetaddr);
        #   Build dotted IP address to pass to sfmike
        $IPaddress = "$inetaddr[0].$inetaddr[1].$inetaddr[2].$inetaddr[3]";

        if (defined $hosts{$IPaddress}) {

            #   Check for a BYE packet

            $isbye = 0;
            if ($pr == 0) {
                if (ord(substr($sockread, 1, 1)) == 2) {
                    $isbye = 1;
                }
            } else {
                $isbye = &isRTCPbye;
            }
            if ($isbye) {
                if ($debug) {
                    print "BYE received from $IPaddress\n";
                }

                #   If child process still active, kill it.  This allows
                #   the user to end the transmission at any time by
                #   disconnecting.

                if (!$live && ($timer{$hosts{$IPaddress}} == 0)) {
                    if ($debug) {
                        printf "Killing process $hosts{$IPaddress}\n";
                    }
                    kill('INT', $hosts{$IPaddress});
                }
                &closeout($IPaddress);
                &updlive();
                if ($verbose) {
                    print "$me: $IPaddress bye.\n";
                }
                next;
            }

            #   If we're in the process of timing out this connection,
            #   reset the timer every time we receive a new packet.
            #   This keeps us from timing out the host and inadvertently
            #   restarting the transmission.

            if ($timer{$hosts{$IPaddress}} != 0) {
                $timer{$hosts{$IPaddress}} = time();
            }
            next;
        }

        #   Only look up the host name if we're in verbose mode or
        #   writing a log file.  Host lookups can take a while and
        #   there's no need to create the extra network traffic unless
        #   we really need the host name.

        if ($log || $verbose) {
            $name = "";
            ($name, $aliases, $length, @addrs) = gethostbyaddr($inetaddr,
                length($inetaddr));
            if (length($name) == 0) {
                $name = $IPaddress;
            }
            if ($verbose) {
                print "$me: $name ($IPaddress) $protoName[$pr] connect.\n";
            }

            #   Write a log file entry in a format strongly resembling
            #   NCSA Common HTTPD log file format.  We always use GMT
            #   and zero for the length of the transmission.  Suitable
            #   ugly hacks could remove these limitations.  In place
            #   of "HTTP" we show the protocol we used for the transmission.

            if ($log) {
                ($ss, $mm, $hh, $mday, $mon, $yy, $wd, $yd, $isdst) =
                    gmtime(time());
                print LOGFILE 
                    sprintf("%s - - [%02d/%s/%d:%02d:%02d:%02d +0000] \"GET %s %s/1.0\" 200 0\n",
                        $name,
                        $mday, $mname[$mon], $yy + 1900, $hh, $mm, $ss,
                        $soundfile, $protoName[$pr]);
            }
        }

        #   Now we're actually ready to do something.  Fork a child
        #   process and invoke sfspeaker (or whatever program the user
        #   specified with the "-r" option) to play whatever was
        #   specified on our command line.  Note that we include
        #   the protocol of the request we received on the command
        #   line in order to respond in the same protocol as that
        #   of the request.

        if (!$live && (($child = fork()) == 0)) {
            $SIG{'INT'} = 'killed';
            $zexec = "$program $proto[$pr] $moptions -p$IPaddress/$port $soundfile";
            if ($debug) {
                print("Exec: $zexec\n");
            }
            exec($zexec);
            exit;
        }
        $con++;

        #   Save information about the request in progress:
        #
        #   $children{$child_process_pid} = IP address of host
        #
        #   $timer{$child_process_pid}    = 0 while transmission is
        #                                   underway.  When the child process
        #                                   exits, this is set to the time
        #                                   the process exited, and is updated
        #                                   every time we get another ID
        #                                   packet from the host.  This is
        #                                   used by the timer to timeout
        #                                   hosts that go away without sending
        #                                   a BYE.
        #
        #   $hosts{$IPaddress}            = Child process serving the request
        #                                   from that IP address.

        $children{$child} = $IPaddress;
        $timer{$child} = 0;
        $hosts{$IPaddress} = $child;
        &updlive;
    }

#   &closeout(ip)  --  Close out host with given IP address

sub closeout {
    local($h) = $_[0];
    local($ch) = $hosts{$h};
    delete $children{$ch};
    delete $timer{$ch};
    delete $hosts{$h};
}

#   &dumpstat  --  Dump state arrays

sub dumpstat {
    print "Children:\n"; foreach $s (keys(%children)) { print "  $s $children{$s}\n"; }
    print "Hosts:\n"; foreach $s (keys(%hosts)) { print "  $s $hosts{$s}\n"; }
    print "Timer:\n"; foreach $s (keys(%timer)) { print "  $s $timer{$s}\n"; }
}

#   &killed  --  Catch interrupt when user disconnects before
#                we're done playing the sound.

sub killed {
    exit;
}

#   &reaper  --  Catch terminating child processes and start
#                the inactivity timeout running.

sub reaper {
    local($pid);

    if ($debug) {
        print "Reaper...\n";
    }
    while (1) {
        $pid = waitpid(-1, $WNOHANG);
        if ($debug) {
            print "   Reaped process $pid\n";
        }
        last if ($pid < 1);
        if ($live && $pid == $lchild) {
            $lchild = -1;
            &updlive();
        } elsif (defined $timer{$pid}) {
            $timer{$pid} = time();
        }
    }
    if ($debug) {
        print "Reaped.\n";
    }
    $SIG{'CHLD'} = 'reaper';          # Reset child process reaper
}

#   &tick  --  Scan the list of open connections and check for any
#              which haven't sent an identity packet in $host_timeout
#              seconds.  If that's the case, terminate the connection
#              (rendering it eligible for re-connection if and when we
#              see another packet from this host).

sub tick {
    local($t, $h, $l);

    if ($debug) {
        print("Tick...\n");
    }
    $t = time();
    foreach $h (keys(%children)) {
        if ($timer{$h} != 0) {
            $l = time() - $timer{$h};
            if ($l > $host_timeout) {
                &closeout($children{$h});
                &updlive();
                if ($verbose) {
                    print "$me: $IPaddress timeout.\n";
                }
            }
        }
    }
    alarm(10);
    $SIG{'ALRM'} = 'tick';            # Reset timeout handler
}

#   &isRTCPbye  --  See if a received packet is an RTCP BYE

sub isRTCPbye {
    local($p0, $p1, $len, $n, $end, $sawbye);

    $sawbye = 0;
    $len = length($sockread);
    $p0 = ord($sockread);
    $p1 = ord(substr($sockread, 1, 1));
    if ((($p0 >> 6) == 2 || ($p0 >> 6) == 1) &&
        (($p0 & 0x20) == 0) &&
        (($p1 == 200) || ($p1 == 201))) {
    }

    $n = 0;
    do {
        if (ord(substr($sockread, $n + 1, 1)) == 203) {
            $sawbye = 1;
        }
        $n += (((ord(substr($sockread, $n + 2, 1)) * 256) +
                 ord(substr($sockread, $n + 3, 1))) + 1) * 4;
    } while (($n < $len) && ((ord(substr($sockread, $n, 1)) >> 6) == 2));
    $n == $len && $sawbye;
}

#   &updlive  --  Update list of active live audio destinations

sub updlive {
    local($a, $b, $zexec);

    if ($live) {
        if ($lchild >= 0) {
            kill('INT', $lchild);
        } else {
            $a = "";
            foreach $b (keys(%hosts)) {
                if (length($a) > 0) {
                    $a .= " ";
                }
                $a .= "-p$b/$port";
            }
            if (length($a) > 0) {
                if (verbose) {
                    print "$me: sending to $a.\n";
                }
                if (($lchild = fork()) == 0) {
                    $SIG{'INT'} = 'lkilled';
                    $zexec = "$program $moptions $a";
                    if ($debug) {
                        print("Exec: $zexec\n");
                    }
                    exec($zexec);
                    exit;
                }
            } else {
                if (verbose) {
                    print "$me: idle.\n";
                }
            }
        }
    }
}

#   &lkilled  --  Catch interrupt when live audio player terminates

sub lkilled {
    exit;
}

#   &hexdump  --  Dump contents of string in hexadecimal

sub hexdump {
    local($d, $xdp) = @_;
    local($adr) = 0;
    local($l) = 0;

    while (length($d) > 0) {
        if ($l == 0) {
            printf("%s%04X: ", $xdp, $adr);
        }
        if ($l == 8) {
            printf(" :");
        }
        printf(" %02X", unpack('C', $d));
        $d = substr($d, 1);
        $adr++;
        $l = ($l + 1) % 16;
        if ($l == 0) {
            print("\n");
        }
    }
    if ($l > 0) {
        print("\n");
    }
}
