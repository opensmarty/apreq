package TestApReq::inherit;
use Apache::Cookie;
use base qw/Apache::Request Apache::Cookie::Jar/;
use strict;
use warnings FATAL => 'all';
use APR;
use Apache::RequestRec;
use Apache::RequestIO;
use Devel::Peek;
sub handler {
    my $r = shift;
    $r = __PACKAGE__->new($r); # tickles refcnt bug in apreq-1
    Dump($r);
    die "Wrong package: ", ref $r unless $r->isa('TestApReq::inherit');
    $r->content_type('text/plain');
    my $j = Apache::Cookie->jar($r->env);
    my $req = bless { r => $r, j => $j };
    $req->printf("method => %s\n", $req->method);
    $req->printf("cookie => %s\n", $req->cookie("apache")->as_string);
    return 0;
}

sub DESTROY { $_[0]->print("DESTROYING ", __PACKAGE__, " object\n") }

1;