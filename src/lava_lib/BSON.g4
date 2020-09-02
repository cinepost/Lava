lexer grammar BSON;

/*
*BSON_VALUE
*	: STRING | INTEGER | NUMBER | BSON_MAP | BSON_ARRAY
*	;
*
*BSON_KEY
*	: QUOTED | INTEGER | NUMBER
*	;
*
*BSON_MAP
*	: JID_MAP_BEGIN  (BSON_KEY JID_KEY_SEPARATOR BSON_VALUE) (JID_VALUE_SEPARATOR BSON_KEY JID_KEY_SEPARATOR BSON_VALUE)*? JID_MAP_END
*	;
*
*BSON_ARRAY
*	:	JID_ARRAY_BEGIN BSON_VALUE (JID_VALUE_SEPARATOR BSON_VALUE)*? JID_ARRAY_END
*	;

BSON_ARRAY
	: JID_ARRAY_BEGIN .*? BSON_ARRAY*? JID_ARRAY_END
	;


JID_NULL 						: '\u0000';
JID_MAP_BEGIN 			: '\u007b'; // '{'
JID_MAP_END 				: '\u007d'; // '}'
JID_ARRAY_BEGIN 		: '\u005b'; // '['
JID_ARRAY_END 			: '\u005d'; // ']'
JID_BOOL 						: '\u0010';
JID_INT8 						: '\u0011';
JID_INT16 					: '\u0012';
JID_INT32 					: '\u0013';
JID_INT64 					: '\u0014';
JID_REAL16 					: '\u0018';
JID_REAL32 					: '\u0019';
JID_REAL64 					: '\u001a';
JID_UINT8 					: '\u0021';
JID_UINT16 					: '\u0022';
JID_STRING 					: '\u0027';
JID_FALSE 					: '\u0030';
JID_TRUE 						: '\u0031';
JID_TOKENDEF 				: '\u002b'; // triggers on-the-fly string definition
JID_TOKENREF 				: '\u0026'; // references a previous defined string
JID_TOKENUNDEF 			: '\u002d';
JID_UNIFORM_ARRAY 	: '\u0040';
JID_KEY_SEPARATOR 	: '\u003a'; // ':'
JID_VALUE_SEPARATOR : '\u002c'; // ','
JID_MAGIC 					: '\u007f';