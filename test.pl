#!/usr/bin/perl -w

use strict;
use Getopt::Long;
use File::Basename;

#------------------------------------------------------
# Command line processing

my %progs;
my $njobs = 1;
my $nrep;
my $nproc;
my $qsize;
my $mode = 'MOT';
my $flags = '';
my $nloops = '50000';
my $file = '';

sub usage {
  print STDERR <<EOF
Usage:
  ./test.pl [..progs..] -q <quantum-size> -m <mode> -X <rundetopts>
                        -j <njobs> -n <nrep> -p <nproc> {--loops n}
                        {--file <bigfile>}
Where:
  -q  quantum size
  -m  deterministic execution mode (optional, defaults to 'MOT')
  -X  extra options for rundet (optional, defaults to '')

  -j  number of parallel jobs (optional, defaults to 1)
  -n  number of repititions (should be >= 2)
  -p  number of threads
  --loops  loop size (optional, defaults to 50000)
  --file   a big file (required for racey-readfile)

Examples:
  ./test.pl basic nomutex -n 100 -p 16 -q 10000 --loops 50000
  ./test.pl all -n 100 -p 16 -q 10000 -m MOT -f ENDQ,TICK
EOF
  ;
  exit 1;
}

GetOptions(\%progs,
           'q=i' => \$qsize,
           'm=s' => \$mode,
           'j=i' => \$njobs,
           'n=i' => \$nrep,
           'p=i' => \$nproc,
           'X=s' => \$flags,
           'loops=i' => \$nloops,
           'file=s' => \$file,
           'help' => sub { usage(); });

if (!defined($nrep) or !defined($nproc) or !defined($qsize)) {
  usage();
}

if ($njobs < 1) {
  print STDERR "Bad value for -j ($njobs).\n";
  usage();
}
if ($nrep < 2) {
  print STDERR "Bad value for -n ($nrep).\n";
  usage();
}

while (my $p = shift @ARGV) {
  if ($p ne "all" && !-f "racey-$p.c") {
    print STDERR "Unkown benchmark 'racey-$p'\n";
    usage();
  }
  $progs{$p} = 1;
}

if ($progs{all}) {
  delete $progs{all};
  for my $p (`ls racey-*.c`) {
    $p =~ s/racey-(.*)\.c/$1/;
    chomp($p);
    $progs{$p} = 1;
  }
}

if (!keys(%progs)) {
  print STDERR "No programs specified.\n";
  usage();
}

if ($progs{readfile} and $file eq '') {
  print STDERR "No file specified for racey-readfile.\n";
  usage();
}

#------------------------------------------------------
# Build

system("cd ../tools; make rundet dmpshim; cd ../test");
system("make");

#------------------------------------------------------
# Run (why did I do this in perl? ugh.)

my @pipes = ();

sub startprog($) {
  my($prog) = @_;

  my $rundet= "../tools/obj/rundet -q $qsize -m $mode $flags";
  if ($prog eq 'readfile') {
    my $dir = dirname($file);
    $rundet = "$rundet --shim=\"dmpshim --localdir=$dir,5,5,5,5\"";
  }

  my $fh;
  open($fh, "-|", "$rundet obj/racey-$prog $nproc $nloops $file");
  push(@pipes, $fh);
}

sub wait4prog() {
  my $fh = shift(@pipes);
  local($/);  # disable the input record separator
  my $in = <$fh>;
  return $in;
}

sub testprog($) {
  my($prog) = @_;
  print "TESTING: racey-$prog -m $mode -n $nrep -p $nproc -q $qsize --loops=$nloops --file=$file\n";

  my $goodpid = undef;
  my $sig = undef;
  my ($diff, $next);
  $diff = $nrep / 10;
  $diff = 10 if (5 < $diff && $diff < 10);
  $diff = 50 if ($diff > 50);
  $next = $diff;

  my ($started,$done) = (0, 0);
  while ($done < $nrep) {
    while ($started < $nrep && scalar(@pipes) < $njobs) {
      $started += 1;
      startprog($prog);
    }
    my $in = wait4prog();
    $done += 1;
    my $pid;
    if ($in =~ /^rundet: app{pid=(\d+) /m) {
      $pid = $1;
    }
    if ($in !~ /^.*Short signature:.*$/m) {
      print STDERR "Bad output?\nI ran $prog $nproc $nloops\n";
      print STDERR "-----------------------------------\n";
      print STDERR $in;
      return 0;
    }
    my $s = $&;  # the matched string (god i hate perl ...)
    if (!defined($sig)) {
      $sig = $s;
    }
    if ($s ne $sig) {
      print STDERR "Failed at iteration $done:\n${sig} pid=$goodpid\n${s} pid=$pid\n";
      return 0;
    }
    $goodpid = $pid;
    if ($done >= $next) {
      print "OK: $done of $nrep\n";
      $next += $diff;
    }
  }

  print "OK.\n";
  return 1;
}

for my $p (keys %progs) {
  testprog($p);
}
