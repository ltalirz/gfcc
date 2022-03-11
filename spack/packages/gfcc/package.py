# Copyright 2013-2022 Lawrence Livermore National Security, LLC and other
# Spack Project Developers. See the top-level COPYRIGHT file for details.
#
# SPDX-License-Identifier: (Apache-2.0 OR MIT)

from spack import *

class Gfcc(CMakePackage,CudaPackage):
    """Green's Function Coupled Cluster Library"""

    homepage = "https://github.com/spec-org/gfcc"
    url      = "https://github.com/spec-org/gfcc/archive/235216d80f25f36b5448d97f11daa5510282bac1.zip"

    tags = ['ecp', 'ecp-apps']

    version('develop', branch='develop')

    depends_on('tamm')
    depends_on('cmake@3.18:')
    depends_on('tamm +cuda', when='+cuda')
    depends_on('cuda@10.2:', when='+cuda')
    conflicts('%gcc@11:', msg='Early gfcc version took 10x longer to compile with GCC 11.2.0')

    def cmake_args(self):
        args = [
        ]

        return args