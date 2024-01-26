module;
export module fst:common_inputs;
import stl;

namespace infinity {

const u8 COMMON_INPUTS[256] = {
    84,  // '\x00'
    85,  // '\x01'
    86,  // '\x02'
    87,  // '\x03'
    88,  // '\x04'
    89,  // '\x05'
    90,  // '\x06'
    91,  // '\x07'
    92,  // '\x08'
    93,  // '\t'
    94,  // '\n'
    95,  // '\x0b'
    96,  // '\x0c'
    97,  // '\r'
    98,  // '\x0e'
    99,  // '\x0f'
    100, // '\x10'
    101, // '\x11'
    102, // '\x12'
    103, // '\x13'
    104, // '\x14'
    105, // '\x15'
    106, // '\x16'
    107, // '\x17'
    108, // '\x18'
    109, // '\x19'
    110, // '\x1a'
    111, // '\x1b'
    112, // '\x1c'
    113, // '\x1d'
    114, // '\x1e'
    115, // '\x1f'
    116, // ' '
    80,  // '!'
    117, // '"'
    118, // '#'
    79,  // '$'
    39,  // '%'
    30,  // '&'
    81,  // "'"
    75,  // '('
    74,  // ')'
    82,  // '*'
    57,  // '+'
    66,  // ','
    16,  // '-'
    12,  // '.'
    2,   // '/'
    19,  // '0'
    20,  // '1'
    21,  // '2'
    27,  // '3'
    32,  // '4'
    29,  // '5'
    35,  // '6'
    36,  // '7'
    37,  // '8'
    34,  // '9'
    24,  // ':'
    73,  // ';'
    119, // '<'
    23,  // '='
    120, // '>'
    40,  // '?'
    83,  // '@'
    44,  // 'A'
    48,  // 'B'
    42,  // 'C'
    43,  // 'D'
    49,  // 'E'
    46,  // 'F'
    62,  // 'G'
    61,  // 'H'
    47,  // 'I'
    69,  // 'J'
    68,  // 'K'
    58,  // 'L'
    56,  // 'M'
    55,  // 'N'
    59,  // 'O'
    51,  // 'P'
    72,  // 'Q'
    54,  // 'R'
    45,  // 'S'
    52,  // 'T'
    64,  // 'U'
    65,  // 'V'
    63,  // 'W'
    71,  // 'X'
    67,  // 'Y'
    70,  // 'Z'
    77,  // '['
    121, // '\\'
    78,  // ']'
    122, // '^'
    31,  // '_'
    123, // '`'
    4,   // 'a'
    25,  // 'b'
    9,   // 'c'
    17,  // 'd'
    1,   // 'e'
    26,  // 'f'
    22,  // 'g'
    13,  // 'h'
    7,   // 'i'
    50,  // 'j'
    38,  // 'k'
    14,  // 'l'
    15,  // 'm'
    10,  // 'n'
    3,   // 'o'
    8,   // 'p'
    60,  // 'q'
    6,   // 'r'
    5,   // 's'
    0,   // 't'
    18,  // 'u'
    33,  // 'v'
    11,  // 'w'
    41,  // 'x'
    28,  // 'y'
    53,  // 'z'
    124, // '{'
    125, // '|'
    126, // '}'
    76,  // '~'
    127, // '\x7f'
    128, // '\x80'
    129, // '\x81'
    130, // '\x82'
    131, // '\x83'
    132, // '\x84'
    133, // '\x85'
    134, // '\x86'
    135, // '\x87'
    136, // '\x88'
    137, // '\x89'
    138, // '\x8a'
    139, // '\x8b'
    140, // '\x8c'
    141, // '\x8d'
    142, // '\x8e'
    143, // '\x8f'
    144, // '\x90'
    145, // '\x91'
    146, // '\x92'
    147, // '\x93'
    148, // '\x94'
    149, // '\x95'
    150, // '\x96'
    151, // '\x97'
    152, // '\x98'
    153, // '\x99'
    154, // '\x9a'
    155, // '\x9b'
    156, // '\x9c'
    157, // '\x9d'
    158, // '\x9e'
    159, // '\x9f'
    160, // '\xa0'
    161, // '¡'
    162, // '¢'
    163, // '£'
    164, // '¤'
    165, // '¥'
    166, // '¦'
    167, // '§'
    168, // '¨'
    169, // '©'
    170, // 'ª'
    171, // '«'
    172, // '¬'
    173, // '\xad'
    174, // '®'
    175, // '¯'
    176, // '°'
    177, // '±'
    178, // '²'
    179, // '³'
    180, // '´'
    181, // 'µ'
    182, // '¶'
    183, // '·'
    184, // '¸'
    185, // '¹'
    186, // 'º'
    187, // '»'
    188, // '¼'
    189, // '½'
    190, // '¾'
    191, // '¿'
    192, // 'À'
    193, // 'Á'
    194, // 'Â'
    195, // 'Ã'
    196, // 'Ä'
    197, // 'Å'
    198, // 'Æ'
    199, // 'Ç'
    200, // 'È'
    201, // 'É'
    202, // 'Ê'
    203, // 'Ë'
    204, // 'Ì'
    205, // 'Í'
    206, // 'Î'
    207, // 'Ï'
    208, // 'Ð'
    209, // 'Ñ'
    210, // 'Ò'
    211, // 'Ó'
    212, // 'Ô'
    213, // 'Õ'
    214, // 'Ö'
    215, // '×'
    216, // 'Ø'
    217, // 'Ù'
    218, // 'Ú'
    219, // 'Û'
    220, // 'Ü'
    221, // 'Ý'
    222, // 'Þ'
    223, // 'ß'
    224, // 'à'
    225, // 'á'
    226, // 'â'
    227, // 'ã'
    228, // 'ä'
    229, // 'å'
    230, // 'æ'
    231, // 'ç'
    232, // 'è'
    233, // 'é'
    234, // 'ê'
    235, // 'ë'
    236, // 'ì'
    237, // 'í'
    238, // 'î'
    239, // 'ï'
    240, // 'ð'
    241, // 'ñ'
    242, // 'ò'
    243, // 'ó'
    244, // 'ô'
    245, // 'õ'
    246, // 'ö'
    247, // '÷'
    248, // 'ø'
    249, // 'ù'
    250, // 'ú'
    251, // 'û'
    252, // 'ü'
    253, // 'ý'
    254, // 'þ'
    255, // 'ÿ'
};

const u8 COMMON_INPUTS_INV[256] = {
    't',  'e',  '/',  'o',  'a',  's',  'r',  'i',  'p',  'c',  'n',  'w',  '.',  'h',  'l',  'm',  '-',  'd',  'u',  '0',  '1',  '2',  'g',  '=',
    ':',  'b',  'f',  '3',  'y',  '5',  '&',  '_',  '4',  'v',  '9',  '6',  '7',  '8',  'k',  '%',  '?',  'x',  'C',  'D',  'A',  'S',  'F',  'I',
    'B',  'E',  'j',  'P',  'T',  'z',  'R',  'N',  'M',  '+',  'L',  'O',  'q',  'H',  'G',  'W',  'U',  'V',  ',',  'Y',  'K',  'J',  'Z',  'X',
    'Q',  ';',  ')',  '(',  '~',  '[',  ']',  '$',  '!',  '\'', '*',  '@',  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, '\t', '\n', 0x0b,
    0x0c, '\r', 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, ' ',  '"',  '#',  '<',
    '>',  '\\', '^',  '`',  '{',  '|',  '}',  0x7f, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
    0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
    0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
    0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf, 0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
    0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff};

} // namespace infinity