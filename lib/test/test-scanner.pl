#!/usr/bin/perl -w

# test-scanner.pl - test script for the libsensors config-file scanner
# Copyright (C) 2006 Mark M. Hoffman <mhoffman@lightlink.com>
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; version 2 of the License.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
#    MA 02110-1301 USA.
#

require 5.004;

use strict;
use Test::More;
use Test::Cmd;

my ($scenario, $test);
my @scenarios = (
	{ base => 'empty', status => 0,
		desc => 'empty file' },
	{ base => 'comment', status => 0,
		desc => 'one comment line' },
	{ base => 'comment-without-eol', status => 0,
		desc => 'one comment line, without a trailing newline' },
	{ base => 'keywords', status => 0,
		desc => 'keywords with various whitespace variations' },
	{ base => 'non-keywords', status => 0,
		desc => 'various invalid keyword scenarios' },
	{ base => 'names', status => 0,
		desc => 'normal, unquoted names' },
	{ base => 'names-errors', status => 0,
		desc => 'invalid, unquoted names' },
	{ base => 'names-quoted', status => 0,
		desc => 'normal, quoted names' },
	{ base => 'names-quoted-errors', status => 0,
		desc => 'invalid, quoted names' },
);

plan tests => ($#scenarios + 1) * 3;

chomp(my $valgrind = `which valgrind 2>/dev/null`);

if ($valgrind) {
	$test = Test::Cmd->new(prog => "$valgrind --tool=memcheck --show-reachable=yes --leak-check=full --quiet ./test-scanner", workdir => '');
} else {
	diag("Couldn't find valgrind(1), running tests without it...");
	$test = Test::Cmd->new(prog => "test-scanner", workdir => '');
}

foreach $scenario (@scenarios) {
	my ($filename, @stdin, @stdout, @expout, @stderr, @experr, @diff);

	$filename = $scenario->{"base"} . ".conf";
	open INPUT, "< $filename" or die "Cannot open $filename: $!";
	@stdin = <INPUT>;
	close INPUT or die "Cannot close $filename: $!";

	$filename = $scenario->{"base"} . ".conf.stdout";
	open OUTPUT, "< $filename" or die "Cannot open $filename: $!";
	@expout = <OUTPUT>;
	close OUTPUT or die "Cannot close $filename: $!";

	# if stderr file is not present, assume none is expected
	$filename = $scenario->{"base"} . ".conf.stderr";
	if (open ERROR, "< $filename") {
		@experr = <ERROR>;
		close ERROR or die "Cannot close $filename: $!";
	} else {
		@experr = ();
	}

	$test->string($scenario->{"desc"});
	$test->run(stdin => \@stdin);

	# test return status
	ok($scenario->{"status"} == $?, "status: " . $scenario->{"desc"});

	# force the captured outputs into an array - for some reason, the
	# 'standard invocation' of diff_exact() chokes without this
	@stdout = $test->stdout;
	@stderr = $test->stderr;

	# test stdout
	ok($test->diff_exact(\@stdout, \@expout, \@diff),
		"stdout: " . $scenario->{"desc"}) or print @diff;

	# test stderr
	ok($test->diff_exact(\@stderr, \@experr, \@diff),
		"stderr: " . $scenario->{"desc"}) or print @diff;
}

