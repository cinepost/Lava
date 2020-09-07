/*
 * Lava ASCII .LSD file format grammer.
 */

grammar LSD;

import BSON; /* binary json lexer grammar */

file: line*;

line: version | declare | setenv | start | end | property | detail | image | geomerty | time | bgeo | raytrace | quit | defaults | COMMENT ;

bgeo
   : BGEO_START .*? ~'cmd_end' //{_input->LA(1)!='cmd_end'}?
   ;

setenv
   : 'setenv' VAR_NAME '=' VALUE
   ;

version
   : 'cmd_version' VEX_VERSION
   ;

defaults
   : 'cmd_defaults' STRING
   ;

declare
   : 'cmd_declare' OBJECT TYPE VAR_NAME VALUE ( ';' VAR_NAME VALUE )*
   ;

start
   : 'cmd_start' OBJECT
   ;

end
   : 'cmd_end'
   ;

detail
   : 'cmd_detail' ('-T' | (('-v' VALUE) | ('-V' VALUE VALUE)))? OBJNAME  ( 'stdin' | STRING )
   ;

property
   : 'cmd_property' OBJECT VAR_NAME VALUE?
   ;

image
   : 'cmd_image' VALUE VALUE?
   ;

geomerty
   : 'cmd_geometry' VALUE
   ;

time
   : 'cmd_time' VALUE
   ;

raytrace
   : 'cmd_raytrace'
   ;

quit
   : 'cmd_quit'
   ;

COMMENT
   : '#' ~( '\r' | '\n' )*
   ;

OBJNAME
   : '/' NO_QUOTED ('/' NO_QUOTED)?
   ;

TYPE
   : 'float' | 'bool' | 'int' | 'vector2' | 'vector3' | 'vector4' | 'matrix3' | 'matrix4' | 'string'
   ;

OBJECT
   : 'global' | 'material' | 'geo' | 'geomerty' | 'segment' | 'camera' | 'light' | 'fog' | 'object' | 'instance' | 'plane' | 'image' | 'renderer'
   ;

VEX_VERSION
   : 'VEX' INT ('.' INT)*
   ;

VAR_NAME
   : VALID_ID_START VALID_ID_CHAR*
   ;

VALUE
   : INTEGER | NUMBER | STRING
   ;

INTEGER
   : INT
   ;

NUMBER
   : '-'? INT '.' [0-9] + EXP? | '-'? INT EXP | '-'? INT
   ;

STRING
   : QUOTED | NO_QUOTED
   ;

NO_QUOTED
   : ~(' ' | '\'' | '"' | '\t' | '\r' | '\n' | '\u007f')+  // ignore bjson magic number
   ;

QUOTED
   : '"' (NO_QUOTED | ' ')* '"'
   ;

CHARS
   : LETTER+
   ;

BGEO_START
   : '\u007f' VALID_BGEO
   ;

fragment VALID_BGEO
   : 'NSJb'
   ;

fragment VALID_ID_START
   : LETTER | '_'
   ;

fragment VALID_ID_CHAR
   : VALID_ID_START | ('0' .. '9') | '-' | '+' | ':' | '.'
   ;

fragment LETTER
   : [a-zA-Z$_]
   ;

fragment INT
   : '0' | [1-9] [0-9]*
   ;

fragment EXP
   : [Ee] [+\-]? INT
   ;

WS: [ \n\t\r]+
   -> skip;