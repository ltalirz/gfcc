# Copyright 2013-2022 Lawrence Livermore National Security, LLC and other
# Spack Project Developers. See the top-level COPYRIGHT file for details.
#
# SPDX-License-Identifier: (Apache-2.0 OR MIT)

from spack import *

CMAKE_FLAGS = [ '-DUSE_CUDA=ON',
                '-DLINALG_VENDOR=IntelMKL']

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

    def cmake_args(self):
        args = [
        ]

        return args
