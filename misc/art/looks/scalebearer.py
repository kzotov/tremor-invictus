"""Scalebearer: keel's heavy armour under biker's head, the body painted
from scratch: a coat of big overlapping iron scales rimmed in copper over
the back, sides and pauldrons, a chest plate of banded lames with a
copper trim and a glowing gem, gauntlets in small scales, a leather
waist and a wide belt with hanging tassets, worn brown leather trousers
crossed by olive straps, red enamel knee pads, steel greaves and dark
boots with enamel toe caps. Shadows gather where one part meets another.
The head is a pale grey-blue brute with glowing blue eyes under a dark
helm rimmed in copper. Team versions enamel the pads and toe caps and
light the gem and the eyes in the team's colour."""
import math, re
from skins import Sheet, Look, LabelMap
from tremorart import Fbm, Noise, clamp, smooth, mix

# what each texel of keel.tga is, 128 x 128, run-length coded rows: s scale
# coat, p pauldrons, c chest plate, g gauntlets, w waist, t belt and tassets,
# u trousers, n knee pads, r greaves, o boots, k nothing
MAP = LabelMap((
    "o20s11o16k5w9s24u5n7u2t22u2t3uo",
    "o17s18o12k6w8s30n6ut27u2o",
    "o15s21os7k10w3s35n5t27u4",
    "k5s42k10s36n2u3t25u5",
    "k2s47k8s36ku12t12u10",
    "s49c3k5s36u35",
    "s48c6k3s36u35",
    "s47c7k3s36u35",
    "s46c8s39u35",
    "s32c21s40u35",
    "s30c22s40u36",
    "s29c22s41u36",
    "s27c23s42u36",
    "s25c25s42u36",
    "s23c27s42u36",
    "s19c31s42u36",
    "p5s14c31s41u37",
    "p5s14c29ps42u37",
    "p6s12c32s41u37",
    "up6s10c36s38u37",
    "up7s9c38s36u37",
    "up8s7c41s34u37",
    "up9s6c42s33u37",
    "up9s5c43s33u37",
    "u2p8s5c43s5ps27u37",
    "u2p9s3c44s5p4s24u37",
    "u2p9s3c44s5p4s24u37",
    "u2p10sc46s5p3s24u37",
    "u2p10sc46s5p3s24u37",
    "up12c46s5pg2s24u37",
    "up12c46s5g3s24u37",
    "up3g9c46s5g2s25u37",
    "g14c45s3g4s25u37",
    "g13c47s3g3s25u37",
    "g11s2c46s32u31n2u4",
    "g10s3c46s32u26n6u5",
    "g8s6c19s7c19s32u13n3u15nu5",
    "g7s10c11s14c16s33u15nu20g",
    "g6s40c6s40p3u11nu14t6g",
    "s91p10u16t11",
    "s91p10u14t13",
    "s90p12u15t11",
    "ts62ws25p14u16t9",
    "ts11w6s11w8s25w3s22p16u21t4",
    "ts6w34s14w13s17p21u19t3",
    "uw45s2w23s12p23u22",
    "uw78s3p23su22",
    "uw79p24s2u22",
    "u2w78p23s3u22",
    "u2w77p23s5u21",
    "u2w76p20g3s5u22",
    "u2w75p21g3s5u22",
    "u2w74p22g3s5u22",
    "u3w72p15gp6sg3s5u22",
    "u3w72p13g2p7sg3s5u22",
    "u3w72p10g4p8sg2s7u21",
    "u3w70p10g6p8sg2s7u21",
    "u2s3w64psp10g7p8sg3s7u21",
    "us6w3p5g10w36g2w3p4sp8g8p9sg3s7u21",
    "us10p4g20sw6g2w2g19p5sp6g9p10sg2s8u21",
    "s11p3g21s5g25p4s2p7g8p9s2g2s8u21",
    "s11p2g22s5g25p3s4p8g6p9s2g2s7t12u10",
    "s11pg23s5g25s14p4gp11s2gs7t13u10",
    "s11g24s4g26s20p9s3gs5t15u9s",
    "s11g23s5g26s23p6s9t17u6s2",
    "s11g23s5g25ps25g2ps10t18u4s3",
    "s11g23s4w2g24ps25g2ps9t21k2s3",
    "s11g23s4w2g23pgs26gs10t22ks3",
    "s11g23s4w2g26s36t22ks3",
    "s11g23w6g26s35t24s3",
    "s11g23w6g25s35t25s3",
    "s12g22w6g25s32t29s2",
    "s7o12g4o9g2w6g24s8w2s19kst31s2",
    "s5o15go12w3sw3g24s8w2s19k2t31s2",
    "so33w2s3wg23s8w4s17t34s2",
    "o35s4wg23s7w5s16t35so",
    "o36s3wg22s9w4s14t37o2",
    "o36s3wg22s7w7st2s6t41o2",
    "o36s3wg21s9w4s3t49o2",
    "o36s3wg21s10ws6t47u2o",
    "o36s3wg20s18t46u2o2",
    "o20u6o10s3wg20s18t44u4o2",
    "o19u11o7s2wg19s3c2s14t41u8o",
    "o18u12o13s3g17s15t40u9o",
    "o17u14o12s3w7g10s15t38u11o",
    "o16u14o13s3w6g11s15t36u13o",
    "o15u15o12s3gw6g11s15t34u15o",
    "o14u16o12s3gw6g11s15t33u16o",
    "o14u15no12s3gw6g11s15t31u19",
    "uo12u16n2o10s4g2w5g11s15t29u21",
    "uo12u13n5o10s3g3w5g10s16u23t4u23",
    "u2o10u12n7o10s3g3w5g10s12w4u24tu25",
    "u2o9u12n8o9s4g3w5g10s11w5u50",
    "u22n10o6nus4g2w5g11s11w5u50",
    "u22n17u2s2g3w5g10s12w5u50",
    "u30n9u2s2g3w5g10s17u50",
    "u29n10u2s2g3w5g10s17u50",
    "u29n10u2s2g3w5g10s17u50",
    "u29n10u2sg4w5g10s14w4u49",
    "u28n11u2sg4w5g10s14w4u49",
    "u28n2r5n4u2sg4w5g10s14w4u49",
    "u28r8n3u2wg5w4g10s14w4u49",
    "ur2u22r13u3swg5w3g11s13w5u48",
    "ur4u15r19u2s2wg5w3g10s14w4u48",
    "ur7u7r26s3g6w2g10s15w3u48",
    "ur41sw2g6w2g9s12w6u48",
    "ur41w4g6wg10s11w5u49",
    "r42ws3wg16s11w5u49",
    "r43s4wg15s11w5u49",
    "r43s5g15s11g6u48",
    "r44s5wg13s6gs5g5u48",
    "r43o2s3w2g13s6gs5w2g3u48",
    "r31o9r2o3s2w4g11s7g5w2g5u47",
    "r23o22sw6g10ws6g12u7nu39",
    "r8o38w6g10w2s5g12u4n9u34",
    "o8ro37w5g11w2s5g12u3n15u28o",
    "o47g16w2s4g12u5n13u28o",
    "o47g34u5n13u28o",
    "o47g8wg26u4n12u29o",
    "o47g8sg26u5n11u29o",
    "o47g8sg26u5n11u29o",
    "o47g8sg26u5n11u29o",
    "o47g8sg13s3w4g6u6n10u29o",
    "o47g20s3w7g4u7n10u28o2",
    "o47k2g6w5s6cs2w9g3u7n10u28o2",
    "o47k2g5w6s8w11g2u8n9u28o2",
    "o47k3g3w8s6w9s7u6n8u29o2",
    "o47k3g2w9s6w5s12u6n7u14t9u6ko",
))
# how much each texel of keel.tga faces a light from above and in front,
# 16 levels a..p, 128 x 128, run-length coded rows: the model's own form
# baked into the paint
LIT = [[ord(c) - 97 for c, n in re.findall(r"([a-p])(\d*)", row) for _ in range(int(n or 1))] for row in (
    "e6d3c4b3c2dfhmop2o2nmkigf2g4hgfed3efghijmopnig2h2jlmno2p19om2ljhg4f4hij2k2l2m5n7onk3lkhf",
    "e6d4c2d5egkmop3o2n2mlji2hi5gf3g2jlkjiknonig2hijkmno3p18o2n2mkhg3f3ghij2k2l2m5n7onlkl2mki",
    "gfe7f3hi3jkmop5o2n2m2l2k2lm4kj2kmno3mkjkm2ih3ijl2mno5p15o4nmkhg5hi2j2k2l3m4n7o2l2m2n2m",
    "khgf2ghijklmn5op7o2n2m2l3m2n3on3op5onkjklji3jk2lm2n3o7p9o7nkhg2h2i3j2k2l4m3n7o2m2n5",
    "mkjijk2lmn2o2p12o2n2m2l2m2n3o4p8omj2kj5k2l3m3n5o19mkh3i5jk2l5m3n3m2nopo4no2",
    "nlk3l2mn2o2p13o2n2m2l2m2n3o3p10nkj9k3l4m4n6o13nmih4i3j2k2l4m8nop2o3n3",
    "mk4lm2n2o2p13o2n2m2l2m2n3o3p10nki2ji6j4k3l4m4n8o7nmih5i2j2k2l4m8nop2o6",
    "k2jk2lm2n2o3p12o2n2m2l2mn3o4p10nki11j4k3l4m4n9o3n2kih5i2j2k4l3m5lm2o2po2n4",
    "j3k2l2mn3o3p11o2n2m5n2o4p11nki5h4i4j3k4l4m5n10mjh2gh3i2j2k4l3m4l3mnopo2n4",
    "j3k2l2m2n3o3p10o2n2m4n2o4p11omji5h5i4j4k3l3m8n7lig4h2i2j2k4l10mno4n2m2",
    "j4k2l2m2n3o3p9o2n2m2n3o4p12nlji5h7i4j3k3l4m13lhg4h2i2j3k4l7klmno4nm2l",
    "i3j2k2l2m2n3o3p8o2n6o4p12omkji5h2g3h4i3j4k2l9m6lkhg5h2i2j3k3l7klmn2o3nm2l",
    "i4j2k2l2m2n3o3p7o2n4o5p13nkji6h2g4h4i4j2k5l13ki2hg3h2i2j3k4l4k3lm2no3m2lk",
    "h3i2j2k2l2m2n2o5p5o2n3o5p13onji7h2g6h4i3j3k11l4kji2hg3h2i2j4k11lmno2nmjih",
    "h4i2j2k2lm2n3o5p4o8p15onjh9gf2g5h4i2j7k10jhg3f2g2hi3j3k8j2klmno2nlih2",
    "g3h2i2j2kl2mn3o6p3o7p14o3mih9gf4g5h2i7j12igf5g2h2i2j4k6j3klmn2onmk3",
    "g4h2ij2kl2mn5o5p2o5p16o2mlig10f7g3h6i8j5ihfe2f3g2h2i3j5k2j3ijklm2nonml3",
    "hg3h2ij2kl2m2n5o9pop15o2nkjhg10f8g5h8i8hge4f3gh2i4j8i3jklmn3ml3",
    "hg3h2i2jk2lm4n5o7p17o2nmlih2gf9e4f4g9h7i3hfe5f2gh2i4j7i4jklmn3mlkj",
    "fg3h2i2jk2l2m5n5o6p15o5nmlkigf7e7f8g7h6ge6f2g2h2i4j5i3hijklm2n2mkig",
    "fg3h2i2jk2l5m4n3o7p14o6n3mkhg2fe5de9f7g7hgfed2e3f2g2h3i4j2i4h2ijkl2mnmlkgf",
    "h2g2h2i2jk5l3m3n3o9p12o7n3ml2khfe3d4e11f6g6fed3e2f2g2h4i8h4ijklm3lki2",
    "ihg2h2i2j4k3l2m3n5o22no3n6mljgf2ed10e6f5g5fd4e2f2g2h4i6h4ghijklm3lkj2",
    "jih4i2j4k3l3m3n6o19n2o4n6m2lkjfd13e4f5g3fed4e2f2g2h5i3h5g2hijklm3lkj2",
    "jih5i3j3k2l3m4n7o18non9m3l2jfe2d10e4f9ed5e2fg3h5ih4g4hijkl2ml2kj2",
    "jih2gh2i3j3k3l3m5n8o10n14m5l2kj2hedcd7e7f6edc2d2e2f2g4h7g6hijkl3k2j2",
    "i2hg3h2i3j3k2l4m5n29m5l5k2iec4d6e8f3edc4d2e2fg4h5g4f3ghijkl3kj2i",
    "ihg5h2i2j3k3l5m6n24m6l6k2iec5d9e8dc5d2e2fg3h4g3f5ghijk2lk2ji2",
    "ihf3g2h2i3j3k4l4m10n16m8l6k3iec6d11e5dcb2c3d2efg3h3g2f3e3fghijk4j2i2",
    "hgf4g2h2i2j3k5l5m18n4m8l7k4iec12d7e2dcb5c2d2efg2h3gfe4defghij6i2h",
    "gfe3f2g2hi2j5k5l7m25l7k4jhdc15d7cb6c2defg2h3ge2d5fghi2j5ih2",
    "fe5f2gh2i2j6k4l12m18l6k5jiec18d5cba2b4c2defgh3ged6fg2hi7h2",
    "e7f2gh2i4j4k5l15m10l7k6j2hdc2b10c7d4cba4b3cdefgh3gfe2d4efgh4i3h3",
    "d3e5fg2h2i4j5k6l13m6l8k7j2igdcb14c6dcba6b2c2defgh2gfe4d2efg3h7g",
    "d5e4fg2h2i5j4k11lkl17k7j3ihecb17c6ba6b3cd2fgh2gf2e2dcdef2g9f",
    "d6e4fg2h3i4j5k12l14k7j4i2gdb19c5ba7b2c2degh2gf2e2cbce2f4g6f",
    "d7e3f2g2h3i5j6k10l11k6j5i4gcb6a6b8c4ba9b2cdeghgf2edcbcde5fghg2fe",
    "c3d5e3f2g2h4i4j11k5l7k7j5i5hgcba13b7c4b3cd2cb6ceg2fed2cb2cd4fhj2ihfd",
    "c4d5e3f2g3h4i4j10k16j5i4h4fcba15b6c3degik2igdcba2bcd5c2b2cdeghjlmnmkhe",
    "c5d5e3f2g3h4i9j5k14j4i4h5gfca14b8c2dhkmno2nlifcbab2c4d4cdgjklm3n2ljf",
    "c6d5e3f2g4h4i8j5k13j3i4h4g3eba14b7c2dgkno6nkgcb4c2degh2giln4m4lki",
    "dc6d5e2f4g3h4i8j5k3j11i4h4g3fdba13b7c2egkmno7nkfcb3cdfgilm2no4n2ml2k2j",
    "ec7d5e2f4g4h7i4j17i4h4g3ecba12b6c3dfhkmn2o8nkgcb2dfghjnop2o3n2ml2k2j2",
    "fdc8d4e2f5g3h6i5j15i4h4g3eca12b6c3degjlm2n4o6n2kecdfhijkno3nm3l2k3j3",
    "hdcbc7d4e3f4g5h4i5j13i4h4g3eca11b6c4dfhjl2m3n11lifgjk2l2m4lk7j2k2",
    "iecb3c6d3e5f3g5h4i5j11i4h4g2feca10b6c5egijkl2m5n9mkhgh2i4j13k2",
    "jfcb4c5d5e4f3g6h4i4ji2j6i4h4g2feca10b6c4dehijk2l4m13lhg5h4i4j4i2jk2",
    "jgcb5c5d5e3f6g5h4i14h4g2feca9b8c3dfhij2k4l5m10ligf3g3h4i4jih2jk2",
    "khdb7c4d5e3f9g3h4i11h3g3feba8b9c3dfhi2j4k4l5m9jgf4g3h4i4hghik2",
    "kidb7c5d4e7f7g4h3i8h3g2f2dba7b12cdeghi3j5k5l3ml2m5kge3fg4h3i2hgf2hik2",
    "kiecb8c4d4e10f5g4h2i6h3g2f2eca7b12c2egh3i5j5k3l5m5kge3f3g3h4gfe2gijk",
    "kjgcb9c4d8e9f3g3h3i2h3g2fe2dcba8b11cdfg2h5i5j3k3l2kl2m4kgd3ef2g3h2g2fd2egijk",
    "j2hdb9c4d12e7f3g3h4gfe2dcb3a8b11cdefg5h5i4j2k3lk2l3mlkfd4ef2g4fedc2egij2",
    "j2iecb3a2b4c8d10e6f3g3fe2db3a12b9c3ef4g5h6i2j2k6l4jec2def4g2fedcbcegij2",
    "j2igdba4b4c13d8e4f4ecb3a14b9cefe2f7g6h3i2j2k2jk3l3jecdfg2f4e2dcb2ceghj2",
    "j2kjgdba4b3c2bc12d9e5cba15b10cgj2gfe4f5g6hi2j3kj2k2l3jfd2g3fe3dc2babcdfhj2",
    "j2lmlhecbab3c17d12cb3a4b17c2d2jm2jge8f5g2h2i2j6k2l2kif2g2fed4cb2abcdfhij",
    "iknonmkgec2de12d5e4d4c5b15c5b4cegfekm2lifed3e5f4g2h2i6j2k2l2kh3ged2c4ba2bcefhi2",
    "ilno3nlkh2ikl9kjih8gf2edcb9c8d2e2fedc2dfh2gfkmnmlhed5e5f2g2h2i3h2ij2k5ih2gedc2b3a3bcefgh2",
    "jmo5nml2m2n10ml10k2igdc4b4c4d3e2f2g2h2g4h3fekmn2mkfd7e4f2g2h2i2h2i2j2k4jihgedcb4a3bcdefg2",
    "lno5n2m6n8m9nm3lkhed2cb4c2d3e2f2g2h2i4h4fd2kmn2mlifed7e2f3g2h6ij2k2lm2kjhgfe3dcba2bcde2f2",
    "no5n2m3l6m6l6m8lkgedcba2bcd2e3f2gh2i6hg2fcbdkm5ljhge2d4e2f3g2h3gh2i2j2kmn3m2l5kgcb4cdef2",
    "n7m2l2k17l2m7lkhedcbabcd2e2f2g2h2i2j4igecba2dkm5l2kjigfed3e3f2g2h2g2h2ijkln2o8n2kfb4cdefh",
    "n4m3l2k2j3i2j12k2l8kgecba2bcde2f2gh2i2j7hdba3djm4l3k2jihfe3d3e2f2g6hjmno12njeb3cdegk",
    "m4l3k2j3i5h6i5jk3l5kjfdcbabcde2f2gh2i2j2k6ieba3djm3l3k3ji2hg2fedc2d2ef5ghjmo13nmhdbc2dfil",
    "l3k4j2i4h5g10hjk8ifdb4cdef2gh2i2j2k3l3kida4djlml3k3j2i2h3g2fcbc2d2e3f2gjnop8o3n3lhdc2ehjk",
    "j5i3h7g3f4e5fghj7igecb3cdef2g2hi2j2k2l6jda4cil4k3j2i3h3g3db2c3d2e2fhlop9o2n4mkhe2gij2",
    "i3h4g8f3e2d7efgi3j3ihfec2bc2def2gh2i2j2k2l2m3ljeba3bejl2k3j2i3h3g2hgfc7d2ehmop8o3n2m2l2jhgh2i2",
    "g4f8gf2e3d3c6defghi5hfec5defg2h2ij2k2l3m4liecbabdik4j2i3h3g3h2gedc6dfinp9o2nm2l3k2jihg3",
    "f3e8f3e2d2c4b5cdefgh5gfedc4defg2hi2j2k2l2m7kiecbdik3j3i3h3g3h3gec6eglop8o2nm2l3k4jgf3",
    "e3d7ef3ed2c2b9cdeg2h4gfed2c3defgh2i2jk2l2m4n4mljgdehjkj3i3h2g3f2g2hgfdc3dfhknp7o2n2m2l3k2j3ife3",
    "d4c2de2f2hih2fedcb4a2b3c2df2g6fedc2d2efghi2j2k2l2m3n2o2nmlkjhf2ij3i3h2g3f8e3fikmnop5o3n2m2l2k2j4i2fed2",
    "c5efjkl5kjhfdcb2a2b3c3def2g5fed2cd2efghi2jk2l2m2n3o3n2lkjhgfgiji3h2g3f3e3f2ghikmn2o2p5o3n2m2l2k2j2i2h2gfd2c",
    "cd2efikmn2on3mljhgecba2b4c2d2ef2gh3gfed4efghij2k2l2m2n2o4nmlkihgfgi4h2g3f3e3f2hijkmo3p7on3m2l2k2j2i2hg3fed2",
    "efhjkln2o2p3onmkihfdba4b2c2d2e2fghihgfe2d3efghij2kl2m2n2o2p2onmljihgfgi3h2g3f2e5fhj2k2lno2p6on3m2l2k2j2ih2g4f3e",
    "jkl2mn2op6onmkhfeca4b3cd2efghi3hgfe2de2fghijk2l2m2no2p3onmkjihg3i3h2g2f3e3fghjk3jkmnop5o2nm3l2k2j2ih2g2f4gij",
    "l2m2n2o2p6o2nljgeda4b2c2defghiji2hgf2e4fghijk2l2mn2op4onlkjh2g3hi4h2g3f3hjk5j2klnp5o2nml3k2ji2h2g2f5gjl",
    "l3m2n2o3p4o2nmkifdba3b2c2degi2j3i2hg2fe3fghijkl2m2nop5nmljih3g2fgijkl2kj2i2jkl3k3j4mop4on2ml2k2ji2h2gf3e4gjl",
    "k2l2m2n3o7n2mkheba2b2c2deghj6ih2g2fef2ghijkl2mn2op4onlkji2hg2fefiln3m3l7k3j2i2knp3o2nm2lk2ji2h2g2f2e2d3fij",
    "j2k2l2m2n8m2l2jfcb3c2defhijkjk2ji2h2g2f3ghijklm2nop5omlkji2hgfe2fhmn6m3l3k3j3i2jnopo3nmlkj2i2h2g2f2ed5ehi",
    "i3jk2l3m6l3k2jgdcd4efghiji2jkj2i2h2g2f2ghijklm2nop4onmlk2j2igfe3gln5m3l4k2j3i3jno4n2mlkji2h2g2f2ed3c3dgh",
    "gh2ij2k5l2k4j3ihg4f3g2f3efhj2i3h3g4hijklm2nop3o2nml3kjigfe3fjmn3m2l5k3j2i4jno3n2mlkj2ih2gf2e2d2c5dfg",
    "f2gh2ij8i6h2i7hec3d2eg3h5gf2ghijklm2n4mlmo2n2mlkihgfe3hlm4l2k6j3i2h2imon2m2lkj2ih2gfe2d2c8ef",
    "e2f2ghi3h9i4j3k4idb3ded2ef3gh2ge2fghik5jihfehmo2nmlkihgfedefkm2l2k3jk4j2i3h2imn3ml2kj2ih2gfed2c4b4cde",
    "e4fgh3g4f2ghi2j2k3l5jebabc3dfgefh3fd3efgh2gf2ed2c2fmo2nmlkjhgfed2fjl3k2j9i2h4ln3m2lk2jih2gfed2c3b6cd",
    "d4efg3f3ef2gij2k2l4ml3jda3b3cde2fih2fdc2d2ef2ed2cb4emo2nmlkjigfed2fjk3j2i3j5i3hg3ilmnm2lk2jih2gfe2dc2b3a2b3c",
    "c4def3e5gijkl3m7ljdba2b3c2defh2gfc4de3dc2b5emo2nmlkjihfed2eij3i12h2g2fgilm3lk2jih2gfe2dc2b2a4b2c",
    "c5de3d4fik2l2m9ljebab3c2d2eghgfecbc2d2edc2b2ab2cfmo2nmlkjigfed2ehj2i2h6i5h2g4hlnm2l2kj2ihgfe2d2cb2a5b2",
    "cb5cd3c2dgjl2m5n3m4kfcb3c3dfgh2gfecb2c2d2c2b2a2b2cfmo2nmlkjigfed2ehi3h9i3h2g4hlnm2l2kj2ihgf2ed2cb2a4b3",
    "b7c4dfjlm4n9mlhdc4d2fhi2hgfedb3cdc2b2a2b2cdhno3nlkjigfed2egh3g2hg2h4i3h3g3hln2m2lk2jihgf2ed2cb2a4b3",
    "c3b2abc4ehkm2n3o8n2mkfdcd2efhijihfedcb3cdcb2ab3c2flopo2nmljigfed2eg9h3i5h3g2hkmnm2lk2jihgf2ed2cb2a4b2c",
    "df2ecb2c3ehkm2n2o4p4o4nlged2efhjkjigfedcb3cdcb5c2dgmp2o2nmljigfed3f6g2h3i3ji3h3g2im4lk2jihgf2ed2cb2a3b2c2",
    "eilkhe4fikm2n3o2p8o3mjge2fhjk2ihgfedcb3cdb5c2degmp3onmljigfed3ef2e2fgh4i2j2i3h3g2ilm3l2kjihg2fed2cba4b2c2",
    "glnoml3klm5no3p8o3mlkhghkmnmjhgfedbab2c2b3c3d2finp3onmljigfed3e6gh3i3j3i3h5lm3l2kjihg2fe2dcba4b2c2",
    "jmo5n4m2l3mn2o7n4m3l2mno2nkhgfeca2b2cb2c3d2e2glop3onmlkihgfed7egh2i3j2kj2i3h4jlm2l2kjihg2fe2dcba4b2c2",
    "kno7n2mlk16l2mn4o3nkhfedca2bc5d3e2fhmp4on2lkjihgf3dc2d2fh2i3j3k2j2i2h4ikm3lk2jihgfe2dcba4b2c2",
    "jmo7n2mlkj9i5jkln4o4nkhfedba2bc5d2e2f2inp4o2nmlkjihg2fec2defhi4j3k2j2i3h4ilm2l2kjihgfe2dcba4b2c2",
    "jmno6nmlkj4i8h2ijkmn5o3nkhfecba2bcdc3de2f2ginp4o2ml2kjihg3fd2e2ghi2j4k4j2i3h4lm2l2kjihgf2edcba4b2cd",
    "jmno5n2mlkj3i2h9ijlmn5o3mjgfdcba2bcd5ef2g2inp4o2ml2kjihg3fe3fgi2j5k2lk2ji3h4km2l2kjihgf2edb2a3b2c2d",
    "jmn3o3nmlkj3i3h6gh2ijlm2n5onmjgedcba2bcd4e2f2g2inop4onl2kjihg4f3ghij4k4l2kj2i3h2gilml2kjihgf2edb2a3b2c2d",
    "ilmn6mkj3i3h2g7hiklm5n4mjgedcba2bcd3e2f3g2imop4onmlkjihg6h3j3k5l4kj2i3hghlm2lkjihgf2ecb2a3b2c2d",
    "hlmn5mlj2i5hg4f2gh2jklm5n4mjgedc2bab2cd2e2f3ghilop4onmlkjihg5h3ijk7l4k2ji3h3km2lkjihgf2ecb2a3b2c2d",
    "hkm2n3mlkji5h2g2f4ghijkl2m4n4mjhfedc2b3c2de2f2g2h2knop3onm2kjihg4h2i3jk4l8kj2i3hgilmlk2ihgf2ecb2a3b2c2d",
    "ikm6lji5h3g2f4ghijkl3m4n3mkhgfedcb4cde2f2g2h2jmop3onm2ljihg7h2jk2l7m2l2kj2i2hghkml2kjhgf2ecba4b2c2d",
    "iklm4lkji3h4g2f5ghijk2l4m3nm2ljg2fedcb4cdef2g2h2jmop4onmljihg4ed2efjkl5m5l2k2ji2hghkl3kjihgfecba4b2c2d",
    "ikl5kji2h6g2f2e2fghij2k2l4m5lkhg2fedcb4cdefg2h2jlnp4onmljih3g2e3fhjkl3m8l2kj2ihg2hkl2kjihgfecba3b3c2d",
    "jkl4kji2h6g2f2e2f2ghij2k3l3m4l2kihg2fedcb4cdfg2h2iknop3o2nljih6i2j2k2l3m9lk2jihg3hklkjihgfdcba3b2c3d",
    "jk5j2ih5g4f2ef2gh2i2j2k4l7kjh3gfedb2ab2cefgh2ijmop3o2nljihg2h2ijk6l3m5nm2l2kjihgf2gk3jihgfdcba3b2c4",
    "ik5ji2h8gf4g2h2i3j2k6j5ih4gfecb2ab2ceghi2jmop3o2nljihg2h2ik8l4m3nm2l2kjihgfefjk2jig2edcba3b2c4",
    "k6ji2h4ij3gf4g2h2i3j3i2hg2fe5fgh3gfecba2bcdfghijmop3o2nljihg3hijk8l4m2nm2l2ji2hgfefjk2jigfedcba3b2c3d",
    "k5j2ih4ijk2jgf5g2hi2j4ihgfed2c3d2fgh3gfdcba2bcdfhijmop4onljihg3hjk10l4mn2mlkjihgfe2fjk2jhgfedcba3b2c2de",
    "k5j2ih4i2j3gf6ghi2j2k2ihgfed2c5dgh4gfdcba2bcegijmop4onljihg4ijk5jk4l3mnm2lkjihgfe2fjk2jhgfedcba3b2c2df",
    "k4j2ihg4hi2jigfe3f2ghi2jklkjhgfe2dc5degh4gedba2bcdfhkmnop3onmjigf4hj9k4lm3lkji2hgfe2fjk2ihgfedcba2b3c2dg",
    "k4j2ihf3g2hi3fe5fghi2jl2kjig2fedc6dfh5fecb3cegijlnop3onmkigf5h2i3j5k3lm2l2kjihgf2edfjkjihgfec2ba2b3c2dh",
    "k4j2ihf4g2h3fe5fgh2ikl2kjihgfedc6dfhi3hgecb2cdfgijkmnop2onmkigfe2d4e2fij5k2l4kjih2gfed2fjkjihgfdcb2a2b3c2ei",
    "k3j2i2hge2f2g3e3d2e3fghikm2ljihgfedc7dgh4fdb3cdfhi2jlmop2onmljhfed5efgij6kl2k2jihg3fed2fjkjihgfdcb2a2b3c2ei",
    "j5ih2ge3f2gfd6e2fghiklmlkihgfedc7dfg3fec5efhij2klnp2on2ljhfd5fh2ij8k3jihgf4ed2fjkjihgedcb2a2b3c2ei",
    "j4i2hg2fe3f3d6e2fghijlmlkihgfed2c4d2efg2f2d2f2d2efhijk2lnoponmkiged5fi2j5i2j5ihgf4e2d2fjkjihgedcb2a2b4cfi",
    "i6hg2fd3e3c2d3ef2g2h2ijl2kjhgfed2cde2f6e2d2h2edegijklm2op2nmljhfed2cd2gi2j4i4j2ihg2f2e4d3eij2ihgedcb2a2b4cfh",
    "i4h3gfed4e2c2d2ehijhg3hiklkjig2fed2fg4fe4d2ei2fefhiklmno2p2nmkjhfed2cd2gi12hg2f2e5d4fijihgecb2a3b4cfh",
    "h6g2fedcd2e2cdfghjk2jhg3hjk2jihgfedeg5fe4fgjlkf2ghjklmnop2omlkihfed4egi11h2gf2e5d5eijihfecb2a2b5cgh",
    "h4g3f2edc3dedefghijk2jgf2gij3ihgfe2fgf6efgikmnlg2hijlmno2p2nlkjhge2d4eg2h3i6h2g2f2e5d3c2eijihfdcba3b5cfh",
    "g5f3e2d2c2d3e2fghij2ifef2gij2ihgfefgf7ghjkmn2kg2hijlmnop2onljhgfed10ehi4h3g2f2e5d3c2eijihfdcba3b5cfg",
    "g4f2e4d2c3d3e2fg2hi2fe3fhi3hgf2g2fe5filno3nhfghijlmo2p2omjhgfe2d3eg2ed2efhi3h4g2f2e3d5c2ei3gfdcba2b6ceg",
    "f5e3d3c2bc3d2e2f2gh2gede2gh4g4fe7hmop3mhfg2ijlmop3nkhgf2e3fgilmkih3i3h4g3f2e3d4c3dghgfedcbc4b4cef",
    "f4e3d3c3b3c2d2e2f2ghgdcd2eg8fed6flop3nhfg2ijlmop2olig4h2ikmnp2olji4h4g4f2e4d2c4de3d4ef3edc3def",
    "e6d3c4b4c2d2efg2hgdc3defg4hgfed6fjop3nhg2h2jlmop2nkihijkmn2op5olji2h4g5fe4d3c2d3e5fgh4ged3e3",
    "e6d3c3b5c2d2efij2ifdc2d2eg4hgfed6fjnp3nig2h2jlmop2nkjkmnop10nkih4g5f2e3d4cdefghj2k3l3kig2f3e3",
)]


def lit(u, v):
    """0..1, smoothly sampled from LIT."""
    n = len(LIT)
    x, y = u * n - 0.5, v * n - 0.5
    x0, y0 = max(0, min(n - 1, int(math.floor(x)))), max(0, min(n - 1, int(math.floor(y))))
    x1, y1 = min(n - 1, x0 + 1), min(n - 1, y0 + 1)
    fx, fy = clamp(x - x0), clamp(y - y0)
    top = LIT[y0][x0] + (LIT[y0][x1] - LIT[y0][x0]) * fx
    bottom = LIT[y1][x0] + (LIT[y1][x1] - LIT[y1][x0]) * fx
    return (top + (bottom - top) * fy + 0.5) / 16


IRON = (0.17, 0.18, 0.21)
STEEL = (0.32, 0.34, 0.38)
COPPER = (0.56, 0.3, 0.17)
LEATHER = (0.3, 0.19, 0.12)
DARK_LEATHER = (0.14, 0.09, 0.06)
OLIVE = (0.36, 0.36, 0.2)
GUNMETAL = IRON
ENAMEL = {"": (0.6, 0.08, 0.06), "_red": (0.62, 0.06, 0.05), "_blue": (0.08, 0.2, 0.62)}
LIGHT = {"": (0.3, 0.6, 1.0), "_red": (1.0, 0.3, 0.2), "_blue": (0.3, 0.6, 1.0)}
# how glossy each part is
SHINE = {"s": 0.35, "p": 0.4, "g": 0.25, "c": 0.3, "t": 0.15, "w": 0.05, "u": 0.06, "n": 0.4, "r": 0.4, "o": 0.1}
GEM = (255 / 768, 159 / 768, 12 / 768)           # on the chest plate


def scales(u, v, nu, nv):
    """(depth 0..1 down a scale, distance to its rim 0..) for rows of
    overlapping scales, each row offset by half a scale."""
    y = v * nv
    row = math.floor(y)
    x = u * nu + 0.5 * (row % 2)
    fx, fy = x - math.floor(x) - 0.5, y - row
    shape = fy + 1.8 * fx * fx
    return clamp(shape), 1 - shape


def scale_paint(u, v, nu, nv, k, n):
    depth, rim = scales(u, v, nu, nv)
    if rim < 0.08:
        return [clamp(z * k * (0.8 + 0.4 * n)) for z in COPPER]
    if rim < 0.14:
        return [z * 0.3 for z in IRON]                       # the rim's shadow
    lit = 1.25 - 0.75 * depth                                # lit on top, dark at the foot
    if depth < 0.18:
        lit += 0.5 * (1 - depth / 0.18)                      # a glint along the scale's crown
    ridge = 1.15 if abs(((u * nu + 0.5 * (math.floor(v * nv) % 2)) % 1) - 0.5) < 0.04 else 1
    return [clamp(z * k * lit * ridge) for z in IRON]


def detail_fn(variant):
    n, fine, grain = Fbm(1411, 4, 6), Noise(1415, 256, 256), Noise(1421, 64, 512)

    def f(u, v):
        c = MAP.label(u, v)
        if c == "k":
            return [0, 0, 0, 0]
        x, y, e = u * 512, v * 512, MAP.edge(u, v)
        k = (0.85 + 0.3 * n.at(u * 6, v * 6)) * MAP.shade(u, v, 0.55, 3.5)
        nf = fine.at(x, y)
        if c == "s":
            col = scale_paint(u, v, 22, 30, k, nf)
        elif c == "p":
            col = scale_paint(u, v, 14, 18, k, nf)
        elif c == "g":
            col = scale_paint(u, v, 36, 48, k * 0.9, nf)
        elif c == "c":
            d = math.hypot(u - GEM[0], v - GEM[1]) / GEM[2]
            if d < 1:
                col = [z * (0.3 + 0.7 * (1 - d)) for z in LIGHT[variant]]
            elif d < 1.5:
                col = list(COPPER)                            # the gem's setting
            elif e < 1.6:
                col = [clamp(z * k * (0.8 + 0.4 * nf)) for z in COPPER]   # trim
            else:
                lame = (y % 26) / 26.0
                col = [clamp(z * k * (1.2 - 0.6 * lame) * (0.92 + 0.16 * grain.at(u * 16, v * 128)))
                       for z in IRON]
                if lame < 0.07:
                    col = [z * 0.25 for z in IRON]           # the gap between lames
                elif (x % 26) < 3 and 0.12 < lame < 0.24:
                    col = list(COPPER)                        # rivets
        elif c == "w":
            col = [clamp(z * k * (0.85 + 0.3 * nf)) for z in LEATHER]
            if (y % 18) < 2:
                col = [z * 0.5 for z in col]                  # stitched bands
        elif c == "t":
            band = (y % 40) / 40.0
            if band < 0.4:
                col = [clamp(z * k * (0.85 + 0.3 * nf)) for z in DARK_LEATHER]   # the belt
                if band < 0.06 or band > 0.34:
                    col = list(COPPER)
            else:
                col = scale_paint(u, v, 18, 12.8, k, nf)       # tassets hanging below
        elif c == "u":
            crease = abs(grain.at(u * 10, v * 70) - 0.5) < 0.02
            col = [clamp(z * k * (0.8 + 0.4 * nf) * (0.6 if crease else 1)) for z in LEATHER]
            if nf > 0.75:
                col = [clamp(z * 1.3) for z in col]           # scuffed
            strap = min(abs((x + y) % 150 - 75), abs((x - y) % 170 - 85))
            if strap < 12:
                col = [clamp(z * k * (0.85 + 0.3 * nf)) for z in OLIVE]
                if strap > 10.5:
                    col = [z * 0.5 for z in col]
                elif (x % 30) < 3:
                    col = list(STEEL)                         # rivets on the straps
        elif c == "n":
            if e < 0.8:
                col = list(STEEL)                             # the pad's steel rim
            elif nf > 0.86:
                col = [0.7, 0.68, 0.66]                       # chipped enamel
            else:
                col = [clamp(z * k * (1.1 - 0.3 * v % 1)) for z in ENAMEL[variant]]
        elif c == "r":
            col = [clamp(z * k * (0.9 + 0.2 * grain.at(u * 16, v * 128))) for z in STEEL]
            if (y % 32) < 2:
                col = [z * 0.4 for z in col]                  # plate seams
            elif (y % 32) < 5 and (x % 22) < 3:
                col = list(COPPER)
        else:   # boots
            if (y % 64) > 44:
                col = [clamp(z * k) for z in ENAMEL[variant]]  # the toe cap
                if nf > 0.8:
                    col = [0.7, 0.68, 0.66]
            else:
                col = [clamp(z * k * (0.8 + 0.4 * nf)) for z in DARK_LEATHER]
                if (y % 10) < 2 and abs(x % 40 - 20) < 6:
                    col = [0.5, 0.45, 0.4]                    # laces
        l = lit(u, v)
        shine = SHINE.get(c, 0.0) * l ** 8                     # highlight where the form faces the light
        col = [clamp(z * (0.45 + 0.85 * l) + shine) for z in col]
        return col + [1]
    return f


def glow_fn(colour):
    def f(u, v):
        d = math.hypot(u - GEM[0], v - GEM[1]) / GEM[2]
        k = clamp(1.2 * (1 - d))
        return [colour[0] * k, colour[1] * k, colour[2] * k, 1]
    return f


def neck_detail(u, v):
    return list(IRON) + [0.9]


# ---------------------------------------------------------------- the head (biker_h.tga, 768 px view)

EYES = [(115, 402), (252, 398)]
SKIN = (0.6, 0.66, 0.74)
def helm(x, y):
    """How far (x, y) is inside the helm, in px: the brow and the back of the head."""
    return max(320 - y, x - 390)


def head_grade(u, v):
    x, y = u * 768, v * 768
    if helm(x, y) > 0:
        return [0.4, 0.4, 0.45, 1]
    return [0.62, 0.68, 0.8, 1]                       # warm skin to a cold grey-blue


def head_detail_fn():
    n = Fbm(1419, 4, 6)

    def f(u, v):
        x, y = u * 768, v * 768
        h = helm(x, y)
        k = 0.8 + 0.4 * n.at(u * 8, v * 8)
        if h > 14:
            k *= (0.7 + 0.3 * smooth(14, 60, h)) * (1.5 - 0.9 * smooth(0, 700, y + max(0, x - 390)))
            if abs(x - 150) < 40 and y < 300:
                k *= 1 + 0.8 * (1 - abs(x - 150) / 40)       # light along the dome
            ridge = abs(x - 185) < 10 and y < 300         # a ridge down the brow
            return [clamp(z * k * (2.2 if ridge else 1)) for z in GUNMETAL] + [1]
        if h > 0:
            return list(COPPER) + [1]                      # the helm's rim
        for ex, ey in EYES:
            d = math.hypot((x - ex) / 34, (y - ey) / 16)
            if d < 1:
                return [0.02, 0.03, 0.06, 1 - 0.5 * d]     # a dark socket round the light
        return [clamp(z * k) for z in SKIN] + [0.45]
    return f


def head_glow_fn(colour):
    def f(u, v):
        x, y = u * 768, v * 768
        k = 0
        for ex, ey in EYES:
            k = max(k, 1 - math.hypot((x - ex) / 22, (y - ey) / 10))
        k = clamp(k * 1.6)
        return [colour[0] * k, colour[1] * k, colour[2] * k, 1]
    return f


def look():
    return Look("keel", "scalebearer", {
        "body": Sheet({"": "keel", "_red": "keel", "_blue": "keel"},
                      detail={v: detail_fn(v) for v in ENAMEL},
                      glow={v: glow_fn(LIGHT[v]) for v in LIGHT}, size=512),
        "neck": Sheet({"": "keel_h", "_red": "keel_h", "_blue": "keel_h"}, detail=neck_detail, size=64),
        "head": Sheet({"": "biker_h", "_red": "biker_h", "_blue": "biker_h"}, model="biker",
                      grade=head_grade, detail=head_detail_fn(),
                      glow={v: head_glow_fn(LIGHT[v]) for v in LIGHT}, size=256),
    }, {"lower": {"l_legs": "body"},
        "upper": {"u_torso": "body", "u_neck": "neck"},
        "head": {"h_head": "head"}},
        {"lower": ["tag_torso"], "upper": ["tag_head", "tag_torso", "tag_weapon"], "head": ["tag_head"]},
        head_model="biker")
