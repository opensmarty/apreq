#!/usr/bin/perl
# requires successful ./configure && make
#
# expected usage: cd glue/perl; ../../build/xsbuilder.pl run run
#

use strict;
use warnings FATAL => 'all';
use Apache2;
use Apache::Build;

use Cwd;
cwd =~ m{^(.+httpd-apreq-2)} or die "Can't find base cvs directory";
my $base_dir = $1;
my $src_dir = "$base_dir/src";

sub slurp($$)
{
    open my $file, $_[1] or die $!;
    read $file, $_[0], -s $file;
}

slurp my $config => "$base_dir/config.status";
$config =~ /^s,\@APACHE2_INCLUDES\@,([^,]+)/m && -d $1 or
    die "Can't find apache include directory";
my $apache_includes = $1;
$config =~ m/^s,\@APACHE2_LIBS\@,([^,]+)/m && -d $1 or
    die "Can't find apr lib directory";
my $apr_libs = $1;

my $mp2_typemaps = Apache::Build->new->typemaps;
read DATA, my $grammar, -s DATA;

my %c_macro_cache;
sub c_macro
{
    return $c_macro_cache{"@_"} if exists $c_macro_cache{"@_"};

    my ($name, $header) = @_;
    my $src;
    if (defined $header) {
        slurp local $_ => "$src_dir/$header";
        /^#define $name\s*\(([^)]+)\)\s+(.+?[^\\])$/ms
            or die "Can't find definition for '$name': $_";
        my $def = $2;
        my @args = split /\s*,\s*/, $1;
        for (1..@args) {
            $def =~ s/\b$args[$_-1]\b/ \$$_ /g;
        }
        my $args = join ',' => ('([^,)]+)') x @args;
        $src = "sub { /^#define $name.+?[^\\\\]\$/gms +
                      s{$name\\s*\\($args\\)}{$def}g}";
    }
    else {
        $src = "sub { /^#define $name.+?[^\\\\]\$/gms +
                      s{$name\\s*\\(([^)]+)\\)}{\$1}g}";
    }
    return $c_macro_cache{"@_"} = eval $src;
}



package Apache::Request::ParseSource;
use base qw/ExtUtils::XSBuilder::ParseSource/;

__PACKAGE__->$_ for shift || ();

sub package {'Apache::Request'}

# ParseSource.pm v 0.23 bug: line 214 should read
# my @dirs = @{$self->include_dirs};
sub include_dirs {["$base_dir/src"]}

sub preprocess
{
    # need to macro-expand APREQ_DECLARE et.al. so P::RD can DTRT with
    # ExtUtils::XSBuilder::C::grammar

    for ($_[1]) {
        ::c_macro("APREQ_DECLARE", "apreq.h")->();
        ::c_macro("APREQ_DECLARE_HOOK", "apreq_parsers.h")->();
        ::c_macro("APREQ_DECLARE_PARSER", "apreq_parsers.h")->();
        ::c_macro("APREQ_DECLARE_LOG", "apreq_env.h")->();
        ::c_macro("APR_DECLARE")->();
    }
}
sub parse {
    my $self = shift;

    $self -> find_includes ;
    my $c = $self -> {c} = {} ;
    print "Initialize parser\n" if ($__SUPER__::verbose) ;

    $::RD_HINT++;

    my $parser = $self -> {parser} = Parse::RecDescent->new($grammar);

    $parser -> {data} = $c ;
    $parser -> {srcobj} = $self ;

    $self -> extent_parser ($parser) ;

    foreach my $inc (@{$self->{includes}}) {
        print "scan $inc ...\n" if ($__SUPER__::verbose) ;
        $self->scan ($inc) ;
    }

}



package Apache::Request::WrapXS;
use base qw/ExtUtils::XSBuilder::WrapXS/;
our $VERSION = '0.1';
__PACKAGE__ -> $_ for @ARGV;

sub parsesource_objects {[Apache::Request::ParseSource->new]}
sub new_typemap {Apache::Request::TypeMap->new(shift)}
sub h_filename_prefix {'apreq_'}
sub my_xs_prefix {'apreq_'}

sub makefilepl_text {
    my($self, $class, $deps,$typemap) = @_;

    my @parts = split (/::/, $class) ;
    my $mmargspath = '../' x @parts ;
    $mmargspath .= 'mmargs.pl' ;

    # XXX probably should gut EU::MM and use MP::MM instead
    my $txt = qq{
$self->{noedit_warning_hash}

use ExtUtils::MakeMaker ();

local \$MMARGS ;

if (-f '$mmargspath')
    {
    do '$mmargspath' ;
    die \$\@ if (\$\@) ;
    }

\$MMARGS ||= {} ;


ExtUtils::MakeMaker::WriteMakefile(
    'NAME'    => '$class',
    'VERSION' => '0.01',
    'TYPEMAPS' => [qw(@$mp2_typemaps $typemap)],
    'INC'      => "-I.. -I../.. -I../../.. -I$src_dir -I$apache_includes",
    'LIBS'     => "-L$src_dir/.libs -L$apr_libs -lapreq -lapr-0 -laprutil-0",
} ;
$txt .= "'depend'  => $deps,\n" if ($deps) ;
$txt .= qq{    
    \%\$MMARGS,
);

} ;

}


package Apache::Request::TypeMap;
use base 'ExtUtils::XSBuilder::TypeMap';


# XXX This needs serious work
sub typemap_code
{
    {
        'T_MAGICHASH_SV' => 
         {
             OUTPUT => 'if ($var -> _perlsv) $arg = $var -> _perlsv; else $arg = &sv_undef;',

             c2perl => '(ptr->_perlsv?ptr->_perlsv:&sv_undef)',

             INPUT =>  <<'EOT',
do {
    MAGIC *mg;
    if (mg = mg_find (SvRV($arg), '~'))
        $var = *(($type *)(mg -> mg_ptr));
    else
        croak (\"$var is not of type $type\");
} while(0)
EOT

             perl2c => <<'EOT',
(SvOK(sv) ?                                                        \\
            ((SvROK(sv) && SvMAGICAL(SvRV(sv))) ||                 \\
             (Perl_croak(aTHX_ "$croak ($expect)"),0) ?            \\
                 *(($ctype **)(mg_find(SvRV(sv),'~')->mg_ptr)) :   \\
                  ($ctype *)NULL)                                  \\
          : ($ctype *)NULL)
EOT

             create => <<'EOT',
do {                                                               \\
    sv = (SV *)newHV ();                                           \\
    p = alloc;                                                     \\
    memset (p, 0, sizeof($ctype));                                 \\
    sv_magic ((SV *)sv, NULL, '~', (char *)&p, sizeof (p));        \\
    rv = p -> _perlsv = newRV_noinc ((SV *)sv);                    \\
    sv_bless (rv, gv_stashpv ("$class", 0));                       \\
} while (0)
EOT
            destroy => '    free(ptr)',
         },


        'T_PTROBJ' => 
            {
            'c2perl' => '    sv_setref_pv(sv_newmortal(), "$class", (void*)ptr)',

            'perl2c' =>
q[(SvOK(sv)?((SvROK(sv) && (SvTYPE(SvRV(sv)) == SVt_PVMG)) \\\\
|| (Perl_croak(aTHX_ "$croak ($expect)"),0) ? \\\\
($ctype *)SvIV((SV*)SvRV(sv)) : ($ctype *)NULL):($ctype *)NULL)
],

            'create' => 
q[   rv = newSViv(0) ; \\\\
    sv = newSVrv (rv, "$class") ; \\\\
    SvUPGRADE(sv, SVt_PVIV) ; \\\\
    SvGROW(sv, sizeof (*p)) ;  \\\\
    p = ($ctype *)SvPVX(sv) ;\\\\
    memset(p, 0, sizeof (*p)) ;\\\\
    SvIVX(sv) = (IV)p ;\\\\
    SvIOK_on(sv) ;\\\\
    SvPOK_on(sv) ;
],

            },
        'T_AVREF' => 
            {
            'OUTPUT' => '        $arg = SvREFCNT_inc (epxs_AVREF_2obj($var));',
            'INPUT'  => '        $var = epxs_sv2_AVREF($arg)',
            },
        'T_HVREF' => 
            {
            'OUTPUT' => '        $arg = SvREFCNT_inc (epxs_HVREF_2obj($var));',
            'INPUT'  => '        $var = epxs_sv2_HVREF($arg)',
            },
        'T_SVPTR' => 
            {
            'OUTPUT' => '        $arg = SvREFCNT_inc (epxs_SVPTR_2obj($var));',
            'INPUT'  => '        $var = epxs_sv2_SVPTR($arg)',
            },
        }
}

# force DATA into main package
package main;
1;

__DATA__
{ 
use ExtUtils::XSBuilder::C::grammar ; # import cdef_xxx functions 
}

code:	comment_part(s) {1}

comment_part:
    comment(s?) part
        { 
        #print "comment: ", Data::Dumper::Dumper(\@item) ;
        $item[2] -> {comment} = "@{$item[1]}" if (ref $item[1] && @{$item[1]} && ref $item[2]) ;
        1 ;
        }
    | comment

part:   
    prepart 
    | stdpart
        {
        if ($thisparser -> {my_neednewline}) 
            {
            print "\n" ;
            $thisparser -> {my_neednewline} = 0 ;
            }
        $return = $item[1] ;
        }

# prepart can be used to extent the parser (for default it always fails)

prepart:  '?' 
        {0}

           
stdpart:   
    define
        {
        $return = cdef_define ($thisparser, $item[1][0], $item[1][1]) ;
        }
    | struct
        {
        $return = cdef_struct ($thisparser, @{$item[1]}) ;
        }
    | enum
        {
        $return = cdef_enum ($thisparser, $item[1][1]) ;
        }
    | function_declaration
        {
        $return = cdef_function_declaration ($thisparser, @{$item[1]}) ;
        }
    | struct_typedef
        {
        my ($type,$alias) = @{$item[1]}[0,1];
        $return = cdef_struct ($thisparser, undef, $type, undef, $alias) ;
        }
    | comment
    | anything_else

comment:
    m{\s* // \s* ([^\n]*) \s*? \n }x
        { $1 }
    | m{\s* /\* \s* ([^*]+|\*(?!/))* \s*? \*/  ([ \t]*)? }x
        { $item[1] =~ m#/\*\s*?(.*?)\s*?\*/#s ; $1 }

semi_linecomment:
    m{;\s*\n}x
        {
        $return = [] ;
        1 ;
        }
    | ';' comment(s?)
        {
        $item[2]
        }

function_definition:
    rtype IDENTIFIER '(' <leftop: arg ',' arg>(s?) ')' '{'
        {[@item[2,1], $item[4]]}

pTHX:
    'pTHX_'

function_declaration:
    type_identifier '(' pTHX(?) <leftop: arg_decl ',' arg_decl>(s?) ')' function_declaration_attr ( ';' | '{' )
        {
        #print Data::Dumper::Dumper (\@item) ;
            [
            $item[1][1], 
            $item[1][0], 
            @{$item[3]}?[['pTHX', 'aTHX' ], @{$item[4]}]:$item[4] 
            ]
        }

define:
    '#define' IDENTIFIER /.*?\n/
        {
        $item[3] =~ m{(?:/\*\s*(.*?)\s*\*/|//\s*(.*?)\s*$)} ; [$item[2], $1] 
        }

ignore_cpp:
    '#' /.*?\n/

struct: 
    'struct' IDENTIFIER '{' field(s) '}' ';'
        {
        # [perlname, cname, fields]
        [$item[2], "@item[1,2]", $item[4]]
        }
    | 'typedef' 'struct' '{' field(s) '}' IDENTIFIER ';'
        {
        # [perlname, cname, fields]
        [$item[6], undef, $item[4], $item[6]]
        }
    | 'typedef' 'struct' IDENTIFIER '{' field(s) '}' IDENTIFIER ';'
        {
        # [perlname, cname, fields, alias]
        [$item[3], "@item[2,3]", $item[5], $item[7]]
        }

struct_typedef: 
    'typedef' 'struct' IDENTIFIER IDENTIFIER ';'
        {
	["@item[2,3]", $item[4]]
	}

enum: 
    'enum' IDENTIFIER '{' enumfield(s) '}' ';'
        {
        [$item[2], $item[4]]
        }
    | 'typedef' 'enum' '{' enumfield(s) '}' IDENTIFIER ';'
        {
        [undef, $item[4], $item[6]]
        }
    | 'typedef' 'enum' IDENTIFIER '{' enumfield(s) '}' IDENTIFIER ';'
        {
        [$item[3], $item[5], $item[7]]
        }

field: 
    comment 
    | define
	{
        $return = cdef_define ($thisparser, $item[1][0], $item[1][1]) ;
	}
    | valuefield 
    | callbackfield
    | ignore_cpp

valuefield: 
    type_identifier comment(s?) semi_linecomment
        {
        $thisparser -> {my_neednewline} = 1 ;
        print "  valuefield: $item[1][0] : $item[1][1]\n" ;
	[$item[1][0], $item[1][1], [$item[2]?@{$item[2]}:() , $item[3]?@{$item[3]}:()] ]
        }


callbackfield: 
    rtype '(' '*' IDENTIFIER ')' '(' <leftop: arg_decl ',' arg_decl>(s?) ')' comment(s?) semi_linecomment
        {
        my $type = "$item[1](*)(" . join(',', map { "$_->[0] $_->[1]" } @{$item[7]}) . ')' ;
        my $dummy = 'arg0' ;
        my @args ;
        for (@{$item[7]})
            {
            if (ref $_) 
                {
                push @args, { 
                    'type' => $_->[0], 
                    'name' => $_->[1], 
                    } if ($_->[0] ne 'void') ; 
                }
            }
        my $s = { 'name' => $type, 'return_type' => $item[1], args => \@args } ;
        push @{$thisparser->{data}{callbacks}}, $s  if ($thisparser->{srcobj}->handle_callback($s)) ;

        $thisparser -> {my_neednewline} = 1 ;
        print "  callbackfield: $type : $item[4]\n" ;
        [$type, $item[4], [$item[9]?@{$item[9]}:() , $item[10]?@{$item[10]}:()]] ;
        }


enumfield: 
    comment
    | IDENTIFIER  comment(s?) /,?/ comment(s?)
        {
        [$item[1], [$item[2]?@{$item[2]}:() , $item[4]?@{$item[4]}:()] ] ;
        }

rtype:  
    modmodifier(s) TYPE star(s?)
        {
        my @modifier = @{$item[1]} ;
        shift @modifier if ($modifier[0] eq 'extern' || $modifier[0] eq 'static') ;

        $return = join ' ',@modifier, $item[2] ;
        $return .= join '',' ',@{$item[3]} if @{$item[3]};
        1 ;
	}
    | TYPE(s) star(s?)
        {
        $return = join (' ', @{$item[1]}) ;
        $return .= join '',' ',@{$item[2]} if @{$item[2]};
	#print "rtype $return \n" ;
        1 ;
        }
    modifier(s)  star(s?)
        {
        join ' ',@{$item[1]}, @{$item[2]} ;
	}

arg:
    type_identifier 
        {[$item[1][0],$item[1][1]]}
    | '...'
        {['...']}

arg_decl: 
    rtype '(' '*' IDENTIFIER ')' '(' <leftop: arg_decl ',' arg_decl>(s?) ')'
        {
        my $type = "$item[1](*)(" . join(',', map { "$_->[0] $_->[1]" } @{$item[7]}) . ')' ;
        my $dummy = 'arg0' ;
        my @args ;
        for (@{$item[7]})
            {
            if (ref $_) 
                {
                push @args, { 
                    'type' => $_->[0], 
                    'name' => $_->[1], 
                    } if ($_->[0] ne 'void') ; 
                }
            }
        my $s = { 'name' => $type, 'return_type' => $item[1], args => \@args } ;
        push @{$thisparser->{data}{callbacks}}, $s  if ($thisparser->{srcobj}->handle_callback($s)) ;

        [$type, $item[4], [$item[9]?@{$item[9]}:() , $item[11]?@{$item[11]}:()]] ;
        }
    | 'pTHX'
	{
	['pTHX', 'aTHX' ]
	}
    | type_identifier
	{
	[$item[1][0], $item[1][1] ]
	}
    | '...'
        {['...']}

function_declaration_attr:

type_identifier:
    type_varname 
        { 
        my $r ;
	my @type = @{$item[1]} ;
	#print "type = @type\n" ;
	my $name = pop @type ;
	if (@type && ($name !~ /\*/)) 
	    {
            $r = [join (' ', @type), $name] 
	    }
	else
	    {
	    $r = [join (' ', @{$item[1]})] ;
	    }	            
	#print "r = @$r\n" ;
        $r ;
        }
 
type_varname:   
    attribute(s?) TYPE(s) star(s) varname(?)
        {
	[@{$item[1]}, @{$item[2]}, @{$item[3]}, @{$item[4]}] ;	
	}
    | attribute(s?) varname(s)
        {
	$item[2] ;	
	}


varname:
    ##IDENTIFIER '[' IDENTIFIER ']'
    IDENTIFIER '[' /[^]]+/ ']'
	{
	"$item[1]\[$item[3]\]" ;
	}
    | IDENTIFIER ':' IDENTIFIER
	{
	$item[1]
	}
    | IDENTIFIER
	{
	$item[1]
	}


star: '*' | 'const' '*'
        
modifier: 'const' | 'struct' | 'enum' | 'unsigned' | 'long' | 'extern' | 'static' | 'short' | 'signed'

modmodifier: 'const' | 'struct' | 'enum' | 'extern' | 'static'

attribute: 'extern' | 'static' 

# IDENTIFIER: /[a-z]\w*/i
IDENTIFIER: /\w+/

TYPE: /\w+/

anything_else: /.*/
