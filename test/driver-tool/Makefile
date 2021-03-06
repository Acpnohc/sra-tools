# ===========================================================================
#
#                            PUBLIC DOMAIN NOTICE
#               National Center for Biotechnology Information
#
#  This software/database is a "United States Government Work" under the
#  terms of the United States Copyright Act.  It was written as part of
#  the author's official duties as a United States Government employee and
#  thus cannot be copyrighted.  This software/database is freely available
#  to the public for use. The National Library of Medicine and the U.S.
#  Government have not placed any restriction on its use or reproduction.
#
#  Although all reasonable efforts have been taken to ensure the accuracy
#  and reliability of the software and data, the NLM and the U.S.
#  Government do not and cannot warrant the performance or results that
#  may be obtained by using this software or data. The NLM and the U.S.
#  Government disclaim all warranties, express or implied, including
#  warranties of performance, merchantability or fitness for any particular
#  purpose.
#
#  Please cite the author in any work or product based on this material.
#
# ===========================================================================

default: runtests

TOP ?= $(abspath ../..)

MODULE = test/driver-tool

include $(TOP)/build/Makefile.env

runtests: built-in bogus container good vdbcache

TMPDIR ?= /tmp
TEMPDIR ?= $(TMPDIR)

GOOD =\
	fastq-dump \
	fasterq-dump \
	sam-dump \
	sra-pileup \
	vdb-dump \
	prefetch \
	srapath

CONTAINER =\
	SRP000001 \
	SRX000001 \
	SRS000001 \
	SRA000001 \
	ERP000001 \
	DRX000001

good: $(GOOD)

container: $(CONTAINER)

$(TEMPDIR)/tmp.mkfg:
	echo '/LIBS/GUID = "c1d99592-6ab7-41b2-bfd0-8aeba5ef8498"' >$@

mkfg: $(TEMPDIR)/tmp.mkfg

actual:
	@ mkdir actual

built-in:
	@echo "running built-in tests" ; \
	SRATOOLS_TESTING=1 \
	$(BINDIR)/sratools

$(GOOD): | actual mkfg
	@# SRATOOLS_TESTING=2 skip SDL, sub-tool invocation is simulated to always
	@# succeed, but everything up to the exec call is real
	@echo "testing expected output for dry run of" $@ ;\
	NCBI_SETTINGS=$(TEMPDIR)/tmp.mkfg \
	PATH=$(BINDIR):$$PATH \
	SRATOOLS_TESTING=2 \
	SRATOOLS_IMPERSONATE=$@ \
	$(BINDIR)/sratools SRR000001 ERR000001 DRR000001 2>actual/$@.stderr && \
	diff expected/$@.stderr actual/$@.stderr

bogus: | actual mkfg
	@echo "testing expected output for unknown tool" ;\
	NCBI_SETTINGS=$(TEMPDIR)/tmp.mkfg \
	PATH=$(BINDIR):$$PATH \
	SRATOOLS_IMPERSONATE=rcexplain \
	$(BINDIR)/sratools 2>actual/$@.stderr ; \
	diff expected/$@.stderr actual/$@.stderr

$(CONTAINER): | actual mkfg
	@echo "testing expected output for container" $@ ;\
	NCBI_SETTINGS=$(TEMPDIR)/tmp.mkfg \
	PATH=$(BINDIR):$$PATH \
	SRATOOLS_IMPERSONATE=fastq-dump \
	$(BINDIR)/sratools $@ 2>actual/$@.stderr ; \
	diff expected/$@.stderr actual/$@.stderr

vdbcache: | actual mkfg
	@echo "testing expected output for run with vdbcache" ;\
	NCBI_SETTINGS=$(TEMPDIR)/tmp.mkfg \
	PATH=$(BINDIR):$$PATH \
	SRATOOLS_TESTING=5 \
	SRATOOLS_IMPERSONATE=vdb-dump \
	$(BINDIR)/sratools SRR390728 2>actual/$@.stderr ; \
	diff expected/$@.stderr actual/$@.stderr

.PHONY: runtests bogus container good $(CONTAINER) $(GOOD) vdbcache
.INTERMEDIATE: $(TEMPDIR)/tmp.mkfg
