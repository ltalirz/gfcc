# Copyright 2013-2022 Lawrence Livermore National Security, LLC and other
# Spack Project Developers. See the top-level COPYRIGHT file for details.
#
# SPDX-License-Identifier: (Apache-2.0 OR MIT)

from spack import *


class Tamm(CMakePackage,CudaPackage):
    """Tensor Algebra for Many-body Methods
    
    See https://doi.org/10.48550/arXiv.2201.01257 for further information.
    """

    homepage = "https://github.com/spec-org/gfcc"
    url      = "https://github.com/spec-org/gfcc/archive/235216d80f25f36b5448d97f11daa5510282bac1.zip"
    git      = "https://github.com/spec-org/gfcc.git"

    tags = ['ecp', 'ecp-apps']

    version('develop', branch='develop')

    depends_on('mpi')
    depends_on('intel-oneapi-mkl +cluster')
    depends_on('cmake@3.18:')
    depends_on('cuda@10.2:', when='+cuda')
    depends_on('hdf5 +mpi')
    # Still need to update libint recipe for 2.7.x
    #depends_on('libint@2.7:')

    root_cmakelists_dir = 'contrib/TAMM'

    def cmake_args(self):
        args = [
            # This was not able to detect presence of libint in first test
            #'-DLibInt2_ROOT=%s' % self.spec['libint'].prefix,
            '-DHDF5_ROOT=%s' % self.spec['hdf5'].prefix,
            '-DLINALG_VENDOR=IntelMKL',
            '-DLINALG_PREFIX=%s' % join_path(self.spec['intel-oneapi-mkl'].prefix, 'mkl', 'latest'),
        ]
        if '+cuda' in self.spec:
            args.extend([ '-DUSE_CUDA=ON',
             ])

        return args