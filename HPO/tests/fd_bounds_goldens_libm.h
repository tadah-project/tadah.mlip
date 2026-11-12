// Frozen reference data; do not edit. The results of sin, cos and exp at every
// argument at which the objective of the test case "fd bounds: the in-house
// rule's interior gradients are those of d5457d0, bit for bit"
// (test_fd_bounds.cpp) calls them at the points of its goldens: the centre
// and the two stencil points of each coordinate. Recorded from that test
// case's own calls with glibc 2.43 on x86-64, with which the objective gave
// every golden bit for bit; glibc gives the same results on a CPU with AVX2
// and FMA and under GLIBC_TUNABLES=glibc.cpu.hwcaps=-AVX2,-FMA. The C
// standard does not fix these results to the bit: another C library rounds
// some arguments differently. The test case gives its objective these
// results in place of the C library's, so that the goldens hold bit for bit
// whatever the C library. Each entry is {x, f(x)}, in the order of the first
// call.
#ifndef TADAH_HPO_TESTS_FD_BOUNDS_GOLDENS_LIBM_H
#define TADAH_HPO_TESTS_FD_BOUNDS_GOLDENS_LIBM_H

namespace fd_bounds_goldens_libm {

struct Result { double x, y; };

static const Result kSin[] = {
  {0x1.8p+0, 0x1.feb7a9b2c6d8bp-1},
  {0x1.80003254e6e22p+0, 0x1.feb7b0d19f3aep-1},
  {0x1.7fffcdab191dep+0, 0x1.feb7a293dab8ap-1},
  {0x1.2p+2, -0x1.f47ed3dc7408p-1},
  {0x1.200012dfd694dp+2, -0x1.f47ef3b09e28ep-1},
  {0x1.1fffed20296b3p+2, -0x1.f47eb4081e5f9p-1},
  {-0x1.38p+3, 0x1.47300a1c35344p-2},
  {-0x1.37ffeb8d82342p+3, 0x1.472d9e18cce9fp-2},
  {-0x1.380014727dcbep+3, 0x1.4732761f17e82p-2},
  {0x0p+0, 0x0p+0},
  {0x1.92a737110e454p-19, 0x1.92a737110bad3p-19},
  {-0x1.92a737110e454p-19, -0x1.92a737110bad3p-19},
  {0x1.8p+3, -0x1.12b9af7d765a5p-1},
  {0x1.8000192a7371p+3, -0x1.12b85bb553a5ap-1},
  {0x1.7fffe6d58c8efp+3, -0x1.12bb0344ef26bp-1},
  {0x1.80c49ba5e354p+0, 0x1.fed2e3b600bc1p-1},
  {0x1.7f3b645a1cacp+0, 0x1.fe9b4273dbb58p-1},
  {0x1.2049ba5e353f8p+2, -0x1.f4f9dcbfddac5p-1},
  {0x1.1fb645a1cac08p+2, -0x1.f40132c3b3bedp-1},
  {-0x1.37b020c49ba5ep+3, 0x1.3db62c755edc4p-2},
  {-0x1.384fdf3b645a2p+3, 0x1.50a1f164caf0dp-2},
  {0x1.89374bc6a7efap-9, 0x1.8937251f0e492p-9},
  {-0x1.89374bc6a7efap-9, -0x1.8937251f0e492p-9},
  {0x1.80624dd2f1a9fp+3, -0x1.0d8562dc388cfp-1},
  {0x1.7f9db22d0e56p+3, -0x1.17e3db847c48p-1},
  {0x1.a666666666667p+0, 0x1.fe6517cf78ac8p-1},
  {0x1.599999999999ap+0, 0x1.f392030f9e1bep-1},
  {0x1.2e66666666666p+2, -0x1.fff593d2d5d38p-1},
  {0x1.119999999999ap+2, -0x1.cfccf9bc99d9cp-1},
  {-0x1.2866666666666p+3, -0x1.4ae375888f7cp-3},
  {-0x1.479999999999ap+3, 0x1.73cb6afc8b306p-1},
  {0x1.3333333333334p-3, 0x1.320c9e9dfed23p-3},
  {-0x1.3333333333334p-3, -0x1.320c9e9dfed23p-3},
  {0x1.9333333333334p+3, 0x1.1370a4069ce7ap-5},
  {0x1.6ccccccccccccp+3, -0x1.d6b23a8004f5p-1},
};

static const Result kCos[] = {
  {-0x1p+0, 0x1.14a280fb5068cp-1},
  {-0x1.ffffde7210be9p-1, 0x1.14a29d377e146p-1},
  {-0x1.000010c6f7a0bp+0, 0x1.14a264bf218dp-1},
  {0x1.8p-1, 0x1.769fec655211fp-1},
  {0x1.8000218def417p-1, 0x1.769fd58615c7cp-1},
  {0x1.7fffde7210be9p-1, 0x1.76a003448cc03p-1},
  {0x1.4p+1, -0x1.9a2f7ef858b7dp-1},
  {0x1.400014f8b588ep+1, -0x1.9a2fb12c6a9e8p-1},
  {0x1.3fffeb074a772p+1, -0x1.9a2f4cc43bce6p-1},
  {-0x1.3333333333333p-2, 0x1.e921dd42f09bap-1},
  {-0x1.3332f01754b06p-2, 0x1.e921e72d6f5d5p-1},
  {-0x1.3333764f11b6p-2, 0x1.e921d3586fc03p-1},
  {0x1.f333333333333p+1, -0x1.73ad66234c8eap-1},
  {0x1.f33353ea62dfcp+1, -0x1.73ad0c2292ae7p-1},
  {0x1.f333127c0386ap+1, -0x1.73adc023ee271p-1},
  {-0x1.ff7ced916872bp-1, 0x1.1510c2fe3fda7p-1},
  {-0x1.004189374bc6ap+0, 0x1.14342cd737ba8p-1},
  {0x1.8083126e978d5p-1, 0x1.7646881f5a5d4p-1},
  {0x1.7f7ced916872bp-1, 0x1.76f9381e21125p-1},
  {0x1.4051eb851eb85p+1, -0x1.9af34664de119p-1},
  {0x1.3fae147ae147bp+1, -0x1.996b0f88c7179p-1},
  {-0x1.322d0e5604189p-2, 0x1.e948893e9fbdfp-1},
  {-0x1.34395810624ddp-2, 0x1.e8fb1138fa7e2p-1},
  {0x1.f3b2fec56d5cfp+1, -0x1.724d1a78877e2p-1},
  {0x1.f2b367a0f9097p+1, -0x1.750c3f51064a2p-1},
  {-0x1.e666666666666p-1, 0x1.29d25dabc46a2p-1},
  {-0x1.0cccccccccccdp+0, 0x1.fd8343c886923p-2},
  {0x1.999999999999ap-1, 0x1.64b6bde719865p-1},
  {0x1.6666666666666p-1, 0x1.87996529f9d93p-1},
  {0x1.5p+1, -0x1.bd300b98112c3p-1},
  {0x1.3p+1, -0x1.70c856fdd6b67p-1},
  {-0x1p-2, 0x1.f01549f7deea1p-1},
  {-0x1.6666666666666p-2, 0x1.e0f575d0de5b7p-1},
  {0x1.06147ae147ae1p+2, -0x1.2866a57963574p-1},
  {0x1.da3d70a3d70a4p+1, -0x1.b0dd8bbdd9e15p-1},
};

static const Result kExp[] = {
  {0x1p-4, 0x1.1082b577d34edp+0},
  {0x1.0000218def417p-4, 0x1.1082b7b352525p+0},
  {0x1.ffffbce4217d3p-5, 0x1.1082b33c54501p+0},
  {0x1.0000431bde82dp-4, 0x1.1082b9eed15a7p+0},
  {0x1.ffff79c842fa5p-5, 0x1.1082b100d555fp+0},
  {-0x1.8p+0, 0x1.c8f87724b5c1dp-3},
  {-0x1.8000192a73711p+0, 0x1.c8f84a38ad7f8p-3},
  {-0x1.7fffe6d58c8efp+0, 0x1.c8f8a410c26eap-3},
  {-0x1.8000192a7371p+0, 0x1.c8f84a38ad7fap-3},
  {-0x1.ap-3, 0x1.a1e1d93d687dp-1},
  {-0x1.9fffe4bcad9aep-3, 0x1.a1e1decd7f571p-1},
  {-0x1.a0001b4352652p-3, 0x1.a1e1d3ad51b5ep-1},
  {-0x1.a000da1a93294p-3, 0x1.a1e1acbcb456ep-1},
  {-0x1.9fff25e56cd6cp-3, 0x1.a1e205be21607p-1},
  {0x0p+0, 0x1p+0},
  {0x1.0c6f7a0b5ed8dp-21, 0x1.000008637bf39p+0},
  {-0x1.0c6f7a0b5ed8dp-21, 0x1.ffffef3908a5bp-1},
  {-0x1.2p+3, 0x1.02cf22526545ap-13},
  {-0x1.200012dfd694cp+3, 0x1.02ce89abb408ap-13},
  {-0x1.1fffed20296b3p+3, 0x1.02cfbaf9708ccp-13},
  {-0x1.200012dfd694dp+3, 0x1.02ce89abb4081p-13},
  {0x1.0083126e978d5p-4, 0x1.108b6e03a8098p+0},
  {0x1.fef9db22d0e56p-5, 0x1.1079fd336e745p+0},
  {0x1.010624dd2f1aap-4, 0x1.109426d6eeed9p+0},
  {0x1.fdf3b645a1cacp-5, 0x1.107145367730cp+0},
  {-0x1.80624dd2f1aap+0, 0x1.c8491ec88aedcp-3},
  {-0x1.7f9db22d0e56p+0, 0x1.c9a812e2f11d4p-3},
  {-0x1.80624dd2f1a9fp+0, 0x1.c8491ec88aedep-3},
  {-0x1.9f95810624dd3p-3, 0x1.a1f794a72c276p-1},
  {-0x1.a06a7ef9db22dp-3, 0x1.a1cc1ef4e9751p-1},
  {-0x1.a353f7ced9168p-3, 0x1.a134269802ff5p-1},
  {-0x1.9cac083126e98p-3, 0x1.a28fd433f79d8p-1},
  {0x1.0624dd2f1a9fcp-11, 0x1.0020c6b49bc03p+0},
  {-0x1.0624dd2f1a9fcp-11, 0x1.ffbe7afa4452ep-1},
  {-0x1.2049ba5e353f7p+3, 0x1.007d835eb99fdp-13},
  {-0x1.1fb645a1cac08p+3, 0x1.05261f2641014p-13},
  {-0x1.2049ba5e353f8p+3, 0x1.007d835eb99f5p-13},
  {0x1.199999999999ap-4, 0x1.12381757f7735p+0},
  {0x1.ccccccccccccdp-5, 0x1.0ed00d38ccdfp+0},
  {0x1.3333333333333p-4, 0x1.13f03738ed1cep+0},
  {0x1.999999999999ap-5, 0x1.0d201a422a433p+0},
  {-0x1.9333333333333p+0, 0x1.a7f39366530abp-3},
  {-0x1.6cccccccccccdp+0, 0x1.ec8fb37f20de2p-3},
  {-0x1.6ccccccccccccp+0, 0x1.ec8fb37f20de4p-3},
  {-0x1.9333333333334p+0, 0x1.a7f39366530a9p-3},
  {-0x1.8b33333333333p-3, 0x1.a625e0e1ca73ep-1},
  {-0x1.b4ccccccccccdp-3, 0x1.9da8da817f8adp-1},
  {-0x1.2333333333333p-2, 0x1.8145a2f892fa5p-1},
  {-0x1.f333333333333p-4, 0x1.c540ab8de16c9p-1},
  {0x1.999999999999ap-6, 0x1.067b0d3ebf291p+0},
  {-0x1.999999999999ap-6, 0x1.f35bd21f40adcp-1},
  {-0x1.2e66666666667p+3, 0x1.4a0c470010005p-14},
  {-0x1.1199999999999p+3, 0x1.95e4bbeed26fep-13},
  {-0x1.119999999999ap+3, 0x1.95e4bbeed26f1p-13},
  {-0x1.2e66666666666p+3, 0x1.4a0c47001000fp-14},
};

}  // namespace fd_bounds_goldens_libm

#endif
