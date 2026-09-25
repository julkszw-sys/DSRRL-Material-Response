#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace dsrrl::operators::material_response::generated {
inline constexpr std::uint8_t mtd_spx_feature_specular=2u;
inline constexpr std::uint8_t mtd_spx_feature_bump=4u;
struct mtd_spx_negative_record { std::uint64_t semantic_name_hash; const char *raw_dsr_mtd_sha256; std::uint8_t dsr_only_added_mask; };
inline constexpr char k_mtd_spx_pairwise_artifact_sha256[]="2f102565a9fd35575bdaccaec5905d5407791f016f2cd4bf94730e86d33a43df";
inline constexpr char k_mtd_spx_pairwise_json_sha256[]="4ef47a68ca3ba6dab009f49f543711702fd971cd5a2e141d3c0b673272742cf4";
inline constexpr char k_mtd_spx_pairwise_ptde_source_zip_sha256[]="2afb7a5947d1249028842d53ebb6c0fc1087f1550f1d406eb8054e89f8edafef";
inline constexpr char k_mtd_spx_pairwise_dsr_source_zip_sha256[]="0ef7886e1122f2ded5008fdf0166381e5cd2b7a3961b3564e02ad46ce533ceb0";
inline constexpr std::size_t k_mtd_spx_negative_record_count=118u;
inline constexpr std::array<mtd_spx_negative_record,k_mtd_spx_negative_record_count> k_mtd_spx_negative_v1 = {{
    {0x70b38fc97dba53c2ull,"89076b688246989ed9d7c65da4df4508fd2c1ec13c8f929fd1ec852507f7e4ad",6u}, // A01[D]_Alp.mtd
    {0x880a07c4011689dfull,"f108edcb431b26815f0e3c934b44756b6032a274f5e74ca577f7effc83b778b4",6u}, // A02_vollight[Dn]_Add.mtd
    {0x8a735afbc6063db3ull,"f426b36e5cde4c46febcc2670884734d02cfe51f432202978af1d9b3c7e14059",6u}, // A03_[D][L]_Edge.mtd
    {0xda6c5105833670e8ull,"9bb873e6b497069f744c6d6ec3d3b13a04fb1ad367e20a30228ffeb111bb3c7e",6u}, // A03_cloud[Dn]_Add.mtd
    {0xe69e3bfe25bd5f8aull,"36c3e7d08d84b799f2d43c6643f2666745a5fe0b1642403ce20b980d2e7a0017",6u}, // A03_light_shaft[Dn]_Add.mtd
    {0xb3a06c17a1b4d734ull,"925bb26512d5393fe313c63249e92baf74d931f033d31bdaed0000f543d08a8d",6u}, // A03_Sky00[Dn].mtd
    {0xc8479886d5308e97ull,"997b477a05f164a9cf8bca544bffc7267f392ffbc18f004c339b6cfae0e1acb7",6u}, // A03_Sky01[Dn].mtd
    {0x28201369def122aeull,"002fd2a185c94998903129803b886de7e6fb26e47a1be00075624c90457c4c36",6u}, // A03_Sky02[Dn].mtd
    {0x2d24b67e930c94adull,"997b477a05f164a9cf8bca544bffc7267f392ffbc18f004c339b6cfae0e1acb7",6u}, // A03_Sky10[Dn].mtd
    {0x5e1f28393faf986aull,"37a28ba73a711a895d9038e2812cc0a56b259f8eaad73aab73a184de40845c23",6u}, // A03_Sky11[Dn].mtd
    {0xb7b9265208412c2cull,"3ecb6f609a85d4525e77a5e6887a38512c9dc24352c9983ff92a3c49913d69c4",6u}, // A03_Sky[Dn].mtd
    {0x3385cc3ee83cb814ull,"bd18e7cd5519abe22757ce1b24405141bf5ddec6f017ebd7a2c19b044a30d3d3",6u}, // A03_vollight11[Dn]_Add.mtd
    {0x572f9192f674cfafull,"b5e45e710e566ce112f4ac47adfc012df137f0419c55f2400a043971a210b216",4u}, // A04_blood[DS][L]_Alp.mtd
    {0xc7d7b5e386caf907ull,"d868cba2fd036b5e261c9086a0bae901467406f036068425cdda313ac0fa0293",6u}, // A04_light_shaft[Dn]_Add.mtd
    {0xf6aa09ae93dc05a6ull,"16942fb5fe2fbb91112ed727644ff65740e99159167535c11bf33b59dc921354",6u}, // A05[D].mtd
    {0x7218d64749e9fd46ull,"a7c9b592ea201417e51040dd950117727a02726cd945166a58b4565ef7bc6fcb",6u}, // A05[D][L].mtd
    {0xdc4044d1f13dc9b8ull,"f426b36e5cde4c46febcc2670884734d02cfe51f432202978af1d9b3c7e14059",6u}, // A05[D][L]_Edge.mtd
    {0x38619cf7705fa598ull,"e7ada5d58a5d7251539ba6c66801351b9e54df2ddb29d7170caf99fb803b87c1",6u}, // A05[D]_Edge.mtd
    {0x23a85012e4a77bfcull,"4aae5e0b786f13e2b4a60fd5823b7afdfd0a8522d6ed504e6011a5b7fb61c91f",2u}, // A05[DB][L].mtd
    {0x85c6d6a071dab9f2ull,"6a1ec1668bb5c625f27c1872a8f153acb723cdd2bcff38f9b4059249f3748ac9",2u}, // A05[DB][L]_Edge.mtd
    {0x3adda2c1565389aaull,"c1f07ace824fcc0a5e638fc243e97d4dcf591c7b71926623769989978eb56a2e",2u}, // A05[DB]_Edge.mtd
    {0xe3a0da42608e00ccull,"23ddd6eb2e4e3a15e551232a46d4c6bea3acc22d8e20663e0acf7288deb29c62",6u}, // A05_vollight[Dn]_Add.mtd
    {0x252169c253172f36ull,"ae7bbd43f12e7809b64997a024e6531e2829a27bd27086e4fcb5ae28a4f98d13",6u}, // A05_water02[Dn]_Add.mtd
    {0xf05703ca3f6700acull,"86ebb40b79cbe5c2c837154e6872198c3a30ef1ebf857eb0aa64c935cc9fcb19",6u}, // A06_lava[D][L].mtd
    {0x08b38395bd147ea0ull,"1eaa30bd9c6e1a6bae0df1374a4b21e196c130037eaf9a0278a3c5a7592d4384",6u}, // A06_lava[D][L]_Alp.mtd
    {0x07ad6565b68fb697ull,"c9cd25ab667863197d9a34d93327e34710ec4c64b4707e18d136e91ff6fa9617",6u}, // A06_lava_Scr[D][L].mtd
    {0xffc800c39c01da17ull,"64b41cce0d0ae312e9ee5d166ca6a7cb983e4b9188c87c02c24e5bcdb1dc4db2",6u}, // A07_[D][L].mtd
    {0xcc244b9a4ec7d828ull,"8e1ca64f9b5f834da9abb94ae44e728c2b46bc928b56d736c29d30e07e26a3cd",6u}, // A07_Shadowman_shadow[Dn]Sub.mtd
    {0x2b8f299d774334bdull,"bb97d53d6692f7459b5df00670ecfd3abee48b552ebbeafeba0b0c2adcd2a5f9",6u}, // A07_yuki[D][L].mtd
    {0x34cd0453b3adc4faull,"df69e67ab8b378c64d2b99bdc432dec85b6ad623f04d99b0760f97afb10e3306",6u}, // A07_yuki[D][L]Alp.mtd
    {0x916bec6a71d01023ull,"93b7b44b1cd16ca7874637281d9e0595af24d91b2c47a91c29d8730072d5a038",4u}, // A10_Metal[DS].mtd
    {0xaa82e8366b7e24f5ull,"cca68946decf3ad297592fd366c9f382ad38f4d51fe87db5191e7ac17a8353a9",4u}, // A10_Metal[DS][L].mtd
    {0x79b57b8819edc94full,"c66c0bb9539c1d55ae9c3d331b3c909ba00bac77073450f9671d6df64fe265d8",6u}, // A10_muddiness[D]_Edge.mtd
    {0xed6a90af494065c3ull,"fca73aa1395cdb79f6ebb214f48d6e085d95c5bfac0f9695fcb835cbf15bdd85",6u}, // A10_slime[D][L]_Edge.mtd
    {0x9958f79bb4a6513bull,"06b3998445cd7a7a609d789bfb825c0c7347c10689fe436b3833047bdf22713e",6u}, // A12_Grass[D][L]_Alp.mtd
    {0x482409b40c2b148full,"8084750650e2c9c8d359ec2eac8cd4d169a5ce78e7bafa8019cf2b232f10fbf1",6u}, // A12_Grass[D][L]_Edge.mtd
    {0xd220b3dc7ee98975ull,"7d1d0bd909fc860e2cf232f05a58e4c822d8592712a5677155661440db67479d",6u}, // A12_Grass[D]_Alp.mtd
    {0xed14639b54df4611ull,"cf5f0673a6c4ef82a46e2480e466ce8a89339210882825f5661388d814a71166",6u}, // A12_Grass[D]_Edge.mtd
    {0x23d51bfb719cdcddull,"e551b88d3a2557b4232433c15898e4a92fcb6ee078cb79e946ddd04a04306c3e",4u}, // A12_Grass[DS][L]_Edg.mtd
    {0x71da5813d40b5ce5ull,"948afa282f666e932c42c52b0616ef8eddd34ad1571f9b8174ba9bfed12a8436",4u}, // A12_Grass[DS]_Edg.mtd
    {0xe03beb7b115ab1d4ull,"1ac29c1ee5d3130cfd330c06a2866893ff0f921bd142f1e560ac3934a7c246e9",6u}, // A12_Ground[D[L]].mtd
    {0xa69e8ac1a5d2abceull,"c0c9741e553d43998ab65ba03ca5805139d4774caa3732bfb48ce478d93975b0",6u}, // A12_Ground[D].mtd
    {0xf9f8b91100d2d94eull,"09a461441c8086fabedcac56c0b0be5eae8bbf06c892be08c2635aaa4b9b3790",6u}, // A13_fog[D]_Alp.mtd
    {0xa482e8551170b320ull,"4184d5a27693e1fd925bf8112c56a5eb189b7b736917ad5f52d7243d044198b4",2u}, // A13_musi[DSB]_edge_.mtd
    {0xde71a63ab1e4c75full,"66ef4701089734bd34e51723ab27e787dabd02191066944fab7c45bc44e11763",2u}, // A13_musi_02[DSB]_edge_.mtd
    {0x748c3c50a11c37b3ull,"c132ecf2cbe50297a6dc99b3bfc55cb4694489312ade8f8fd82fc9bb7449fe63",2u}, // A14_lava_M[DB].mtd
    {0x967ad2a425341bdcull,"fa013288fffad2826ce57c70113700227da499d99bf0059c68492e61bceaf96f",2u}, // A14_lava_M[DB]alp.mtd
    {0x37026e48f009974cull,"7f9d3cf1b56bff0a605709dbbd833bb669442d81267d964f348cbb45e81598c4",2u}, // A14_lava_M_02[DB].mtd
    {0x46439d38a395511full,"fb49a5dec04b8612fa0b7528ccb34dce775dbc514a3697a15781edbc0cbd01fe",2u}, // A14_lava_M_03[DB].mtd
    {0xb4b61171bc335072ull,"a1e52fede1108345d33a89a8613f76ac21054679f4dd83856e87a3525d353316",6u}, // A17_Light[D].mtd
    {0xd64e431e6edc4dbeull,"3aa084a3809c2914c353f4656e6a95dc356accc0c426af7c8cd477443587106a",6u}, // A17_Light[NL]_LS.mtd
    {0x95f57ab996c22912ull,"89a85e9e77fe2e85d5ba96318085a8dd026cacf11cde84da2ce9d7c9f1142f72",6u}, // A18_WhitePassage[Dn]_Alp.mtd
    {0x64b188fbb1f8a04bull,"e665a9c6b1d394d164f33c59d23c1dfc45aea6687a6b6a3482cde3a75e603927",6u}, // A19_Division[D]_Alp.mtd
    {0xc98fed157ab80e06ull,"39fff1aba30ff70f5bb2380fb06e91b9764efc698894e3082e0f07fc3c9eb3a7",6u}, // A19_Fly[DSB]_edge.mtd
    {0xd65c18cddded21caull,"0c84bbb215e9e70c6159186e8a6708909fd2790f9e66488239a0ffec8370872c",6u}, // A19_mountains[Dn]_Alp.mtd
    {0xdd764f9266989540ull,"37160a8088a109742ca90b94888e625baeaa61636cec240ff6c249c5b1a1652a",6u}, // C5250_Fire[D]_Add.mtd
    {0xda88471908a99185ull,"7b616d133d88be12a310f287c36b6d82fa4b24eaf7f5048f399f99ed8e48aa6b",6u}, // C[D].mtd
    {0xdd579cf50e78d21dull,"9da111c8dffa8ae00183503b25b09f50eb0723c1aa728173e25a5ebe903b2c80",6u}, // C[D]_Alp.mtd
    {0xe0ec2f8ae6945879ull,"75579c2f856bbdfa83befc403f952b49fee199da933421d1616ece94d45f4900",6u}, // C[D]_Edge.mtd
    {0xdf682b313d2d08b5ull,"a9052ac1d858eb19ba67e4237729dc94bd523e69b5fbcbc0e8839352bfc66118",2u}, // C[DB].mtd
    {0xdd39f36d2bb36171ull,"aa0da3dacd4728707a9c1e6f74cf37520711fe4223102577cf918d099109069a",6u}, // C[DN].mtd
    {0xe8d02f71a28a3841ull,"43341cb30f89a24ec4a27b6d5703f89431be82dbfd9c2c0ea6bd491209f7a4b6",6u}, // M99_cloud11[Dn]_Add.mtd
    {0x836644e145f39887ull,"c4528e7dfd6a771131e758a9f4b7767233215e5f21bc94b66c26f3c0ad081cfe",6u}, // M99_cloudshadow11[D][L].mtd
    {0xb2b9506775636b87ull,"61a815040b605184e15d819b722c8ad92be75461f72ea1e5ccf9c37392ee0978",6u}, // M99_cloudshadow11[D][L]_Edge.mtd
    {0x935a9e91683e644aull,"8b4898c2fdd001de3c3050c1055472f1ea7766b9affb3fc7ca2cd66876a8b72b",6u}, // M99_cloudshadow11[D][ML].mtd
    {0x85d89ba3d2d48dfbull,"7a951c0cb8d7b317dffe913600487a4648054a956dd2a316a001b373b3280d16",4u}, // M99_cloudshadow11[DS][ML].mtd
    {0x2d2f37fd91b6ca50ull,"586f411fbc716aa084c49ad7df52d4e2abba2b0265a21ed910dab47c72755a82",6u}, // M99_lava12a[D][L].mtd
    {0x7bce364d619a3891ull,"c177d5f8eb3216d00ef48d14a8aba9b2af3f0b8d3a400ccc41cae9c49829d6a7",6u}, // M99_lava12b[D][L].mtd
    {0x41195a135c5aabb1ull,"411437194838215efc77054066609a6d4adf53fc6fbdfa17d5e5f057c42538e1",6u}, // M99_vollight11[Dn]_Add.mtd
    {0x73e7d3e063e137e4ull,"9f3b80768c7108a17f04261ea09ae5cfa7d483b31288e4735877565573af1766",6u}, // M99_vollight_dust11[Dn]_Add.mtd
    {0xb829226f69fd5d07ull,"c512b98f0ff48674ebfe429c8252c0a9e8863e4b0edec8e9ef30b60f197356aa",6u}, // M[D].mtd
    {0x16fee56adcc33421ull,"b4e46f5a3f9a65e9ce92d812a996270de2f0e35dbcbe679dcbc8142b66d3f4cd",6u}, // M[D][L].mtd
    {0xe96826b0feeece99ull,"c5563ce66d23155ee28dfd6957441436c6330e220ac860cb91babd575f4da405",6u}, // M[D][L]_Alp.mtd
    {0x86ac0e6b802e2e81ull,"a00b1efd7f997dfaf55c54491cb85e6ab78a1fc7b1e55ff638f35496c924c157",6u}, // M[D][L]_DetB.mtd
    {0x25644d693cf773a5ull,"f426b36e5cde4c46febcc2670884734d02cfe51f432202978af1d9b3c7e14059",6u}, // M[D][L]_Edge.mtd
    {0x05b980faad1ffbf8ull,"fed91ae69cdd4e816c9b078b621f6e3aa7c7e1191d76c8cb1d22a7cb02fbc113",6u}, // M[D][M].mtd
    {0x24e8f6bc501b0c10ull,"525c4840c3bb0a44f2e5ebbd2d090e58e601b68c41d76035f1de417343842daa",6u}, // M[D][ML].mtd
    {0x43319d5424c06d93ull,"bc22808b3d7b353e70414d1473bbecc386eb1f146786ef8b17a7a9fc8ae7663f",6u}, // M[D]_Alp.mtd
    {0xb21f7a4194076907ull,"e224dc42a6b34eaefd0a2a1f64f88e74f89476a0d52509f0c886c0c629e3309c",6u}, // M[D]_Edge.mtd
    {0xb1762f6ef899da9full,"afe70656efd6c7797fccce5a43e7c4738eb3fec5177dd2c787f68438332d238f",2u}, // M[DB].mtd
    {0x08241919054f4699ull,"4aae5e0b786f13e2b4a60fd5823b7afdfd0a8522d6ed504e6011a5b7fb61c91f",2u}, // M[DB][L].mtd
    {0x4b2e0a4a1bbf8071ull,"e00259398b605f907bceac05aff5038e86be8bd3867899dbfc5caed87650e1ba",2u}, // M[DB][L]_Alp.mtd
    {0x1bbfb741cd6dfd79ull,"c3e2085a7a54b0af93ff6f8bb5a265450628e4fa1fd50f956a945b1bb069e091",2u}, // M[DB][L]_DetB.mtd
    {0xea2f460f364d63fdull,"6a1ec1668bb5c625f27c1872a8f153acb723cdd2bcff38f9b4059249f3748ac9",2u}, // M[DB][L]_Edge.mtd
    {0xe412a0511ab193f0ull,"d20c40770dfcdba8497e634723ff17ee35f23c60b39744a4d10785522dcaae6a",2u}, // M[DB][M].mtd
    {0x8ea9bfa331db1728ull,"297586510a28c77c379ac46940bc1355ea4109f844505ab6f6ad8d2621da730f",2u}, // M[DB][ML].mtd
    {0xa0b48515d74f05ebull,"348e204113de1972822a1b509458c4ed9eb99a75f1856a51e9b6dc6dd6a76ca9",2u}, // M[DB]_Alp.mtd
    {0xcab8d645b3eea17full,"3cd6b57d6c572d4efaad33ed1ecc8cbaf5c7beebf3cb4494ff6eb9e0cbfc546e",2u}, // M[DB]_Edge.mtd
    {0x86bf311394e45bc7ull,"3b1ce6088d0aa49a12ac8fb285c77827f426a1f4ee9fb355ffd5b32b6d2a1389",4u}, // M_1Wood[DS].mtd
    {0x3dbb8b9187fe03e1ull,"de42cf7fad48b111357a148b8b8a9d5baed8617a5e4650b44cdfae6ef5762d70",4u}, // M_1Wood[DS][L].mtd
    {0x382d343202aaf559ull,"7ec6192d07052a206494670af1e85fb35ef059d46137c80a66ed7662fa3c82e3",4u}, // M_1Wood[DS][L]_Alp.mtd
    {0x8662271c7340fcb8ull,"b83732268f7332a3888b20e8eb16a2fb03bd4fb46bab25365254ff778228bdf7",4u}, // M_1Wood[DS][M].mtd
    {0x35de1b89b9e07cd0ull,"1a751dace80012a9d4b8720a52c846cfdc8c6178448601ee3a45167ee82bbe71",4u}, // M_1Wood[DS][ML].mtd
    {0xe26c78def8fbec26ull,"d0492773e16838f3f5ddc3a3319cb160b73f830883626a131f70397e57580019",4u}, // M_4Stone[DS].mtd
    {0x6bca5f537a6bf4c6ull,"5ab39d56b336c44063a3918d27b9b5c74c246d91cadea84ecabe8d3ba003f69e",4u}, // M_4Stone[DS][L].mtd
    {0x14840f6f5208332eull,"7e89b0fca47ea83ac6601e9615f5049643b4da71e61abe896acd02935e10b7bf",4u}, // M_4Stone[DS][L]_Alp.mtd
    {0xfa6fb51358bad787ull,"5199e29c6973809348dd00c6e63f276596ab9fd33465d81cb8289712f52c33dc",4u}, // M_4Stone[DS][M].mtd
    {0xcee11689959f2ed9ull,"e4fb89d557e2111856f8a1c4339c3e4e492d9b4ab55dc08bcec1eb962d6ef197",4u}, // M_4Stone[DS][ML].mtd
    {0xc44a72e35f35ec0eull,"64b87a013fab559d5e68c34c454449dc84245d38041e95e6b7430ebcc1264afd",4u}, // M_4Stone[DS]_Alp.mtd
    {0x245689e4f07064cbull,"93b7b44b1cd16ca7874637281d9e0595af24d91b2c47a91c29d8730072d5a038",4u}, // M_7Metal[DS].mtd
    {0x0e768913a1acaeddull,"cca68946decf3ad297592fd366c9f382ad38f4d51fe87db5191e7ac17a8353a9",4u}, // M_7Metal[DS][L].mtd
    {0x7b9226d08be09b35ull,"469a8097d652e715af6173ec14b23f52f6c0ac3de591ea026cc6bbbd7f5997cc",4u}, // M_7Metal[DS][L]_Alp.mtd
    {0xd6096397737c9894ull,"c717aaf76b7970d8bd33e9f44a1d785f4392d207ec748db0acb7e3d1525090fa",4u}, // M_7Metal[DS][M].mtd
    {0xb0371f2bffaa40dcull,"e28cbe02ea4f43ef55f960742246ca35484b6c282dbc9f6e70c77f8f5a32f50e",4u}, // M_7Metal[DS][ML].mtd
    {0x67e64c056d44f9beull,"1b32bdacb81f2c083463f8e0318c9565c22ae54fa8621414c1347165c438d441",4u}, // M_9Glass[DS].mtd
    {0x46be5648b0e5e05eull,"565b149120f9e28513ddc3ad8b109dfc924dd6d8684271b54f754683cb16b0c3",4u}, // M_9Glass[DS][L].mtd
    {0xf351a2b67b3e00e6ull,"be568bec32dbcf900b73b2d71da252765fa23c7f53fdc3c7a659922553b43d55",4u}, // M_9Glass[DS][L]_Alp.mtd
    {0x66c956aaf071b9ffull,"b2082eca957afe1861c46c3ff92a3d9f3ab1c336c46a66dfe088a02140f4493c",4u}, // M_9Glass[DS][M].mtd
    {0x2b90b76bf2638371ull,"4a6b7e33fd6fb5bc39ccc8af26a5939790a106c99a4652ee37e0c9c55ccb682d",4u}, // M_9Glass[DS][ML].mtd
    {0x9eef05fadbfab48aull,"e0e263990784d9cc118e2b3afc895f25de9b5780458a796d38447dc55b1ee17e",6u}, // P[D].mtd
    {0x0dad77e58fb57b70ull,"b9f9d169c16a9dd4e34d2f6bcdd3e195182858a79a0420d7987118cf1ecb3661",2u}, // P[DB].mtd
    {0xc5b80e0fb113e2d3ull,"53819ead337c1ecd8593d8535c1fc1fdde589f00eb8ea015a8ae347da71073ea",4u}, // P_Leather[DS].mtd
    {0x09983523326de25cull,"26126103e9725a9cd92538bc6ef5d34e031b3f6379ce70779ad15ce153cffbac",6u}, // Ps_Hair[D].mtd
    {0x07caf296c3872590ull,"d31ad3b8970c4cbc7fd4a8c60b07809dce44b4a5b7b10ead5bdf07af774e0659",6u}, // Ps_Hair[D]_Alp.mtd
    {0xed502b275d0aa2d2ull,"bddfa93be52410580904b07416c138153bd9a82fd216bf685329028f1a70a441",6u}, // Ps_Hair[D]_Edge.mtd
    {0xaf184fe9bfb687bbull,"c628feb3773af7edb0bec7276f80d7bdb41220aa0c2754dad198b2398b4e82c3",4u}, // Ps_Hair[DS].mtd
    {0xa39ea1d9783b3e47ull,"43ef07edf1fa3ae392aecc2bc02add7ef51f16538eb6b60e438c05a045a8a36e",4u}, // Ps_Hair[DS]_Alp.mtd
    {0x6429af319fb65a1bull,"4eaa551380c3943be768de1e78eca9a43c5a9356ea13e401274ad6c8faef749c",4u}, // Ps_Hair[DS]_Edge.mtd
}};
} // namespace dsrrl::operators::material_response::generated
