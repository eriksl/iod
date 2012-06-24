#!/usr/bin/perl -w

use strict;
use Net::INET6Glue::INET_is_INET6;
use LWP::Simple;
use Data::Dumper;

my($fetcher) = LWP::UserAgent->new;
my($content) = $fetcher->get("http://andromeda.ipv6.slagter.name:4242/get_analog_input?input=1");
#printf("content = %s\n", $content->content);
my($value);
($value) = $content->content =~ m/\[([0-9.+-]+)\]/;
printf("value = %s\n", $value);
