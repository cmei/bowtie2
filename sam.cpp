/*
 * sam.cpp
 */

#include <string>
#include "sam.h"
#include "filebuf.h"
#include "aligner_result.h"

using namespace std;

/**
 * Print a reference name in a way that doesn't violate SAM's character
 * constraints. \*|[!-()+-<>-~][!-~]* (i.e. [33, 63], [65, 126])
 */
void SamConfig::printRefName(
	OutFileBuf& o,
	const std::string& name) const
{
	size_t namelen = name.length();
	for(size_t i = 0; i < namelen; i++) {
		if(isspace(name[i])) {
			return;
		}
		o.write(name[i]);
	}
}

/**
 * Print a reference name given a reference index.
 */
void SamConfig::printRefNameFromIndex(OutFileBuf& o, size_t i) const {
	printRefName(o, refnames_[i]);
}

/**
 * Print SAM header to given output buffer.
 */
void SamConfig::printHeader(
	OutFileBuf& o,
	const string& rgid,
	const string& rgs,
	bool printHd,
	bool printSq,
	bool printPg) const
{
	if(printHd) printHdLine(o, "1.0");
	if(printSq) printSqLines(o);
	if(!rgid.empty()) {
		o.writeChars("@RG");
		o.writeString(rgid);
		o.writeString(rgs);
		o.write('\n');
	}
	if(printPg) printPgLine(o);
}

/**
 * Print the @HD header line to the given OutFileBuf.
 */
void SamConfig::printHdLine(OutFileBuf& o, const char *samver) const {
	o.writeChars("@HD\tVN:");
	o.writeChars(samver);
	o.writeChars("\tSO:unsorted\n");
}

/**
 * Print the @SQ header lines to the given OutFileBuf.
 */
void SamConfig::printSqLines(OutFileBuf& o) const {
	char buf[1024];
	for(size_t i = 0; i < refnames_.size(); i++) {
		o.writeChars("@SQ\tSN:");
		printRefName(o, refnames_[i]);
		o.writeChars("\tLN:");
		itoa10<size_t>(reflens_[i], buf);
		o.writeChars(buf);
		o.write('\n');
	}
}

/**
 * Print the @PG header line to the given OutFileBuf.
 */
void SamConfig::printPgLine(OutFileBuf& o) const {
	o.writeChars("@PG\tID:");
	o.writeString(pg_id_);
	o.writeChars("\tPN:");
	o.writeString(pg_pn_);
	o.writeChars("\tVN:");
	o.writeString(pg_vn_);
	o.write('\n');
}

#define WRITE_SEP() { \
	if(!first) o.write('\t'); \
	first = false; \
}

/**
 * Print the optional flags to the given OutFileBuf.
 */
void SamConfig::printAlignedOptFlags(
	OutFileBuf& o,          // output buffer
	bool first,             // first opt flag printed is first overall?
	bool exEnds,            // exclude ends of sequence?
	const Read& rd,         // the read
	const AlnRes& res,      // individual alignment result
	const AlnFlags& flags,  // alignment flags
	const AlnSetSumm& summ, // summary of alignments for this read
	const char *mapqInp)    // inputs to MAPQ calculation
	const
{
	char buf[1024];
	if(print_as_) {
		// AS:i: Alignment score generated by aligner
		itoa10<TAlScore>(res.score().score(), buf);
		WRITE_SEP();
		o.writeChars("AS:i:");
		o.writeChars(buf);
	}
	if(res.color()) {
		if(print_cs_) {
			// CS:Z: Color read sequence on the original strand
			// Note: the 'primer' and 'trimc' fields of the Read data structure
			// are set to '?' unless the primer base is present in the input.
			WRITE_SEP();
			o.writeChars("CS:Z:");
			if(rd.primer != '?') {
				assert_neq('?', rd.trimc);
				o.write(rd.primer);
				o.write(rd.trimc);
			}
			o.writeString(rd.patFw);
		}
		if(print_cq_) {
			// CQ:Z: Color read quality on the original strand
			WRITE_SEP();
			o.writeChars("CQ:Z:");
			o.writeString(rd.qual);
		}
	}
	if(print_xs_) {
		// XS:i: Suboptimal alignment score
		AlnScore sc = summ.secbestMate(rd.mate < 2);
		itoa10<TAlScore>(sc.valid() ? sc.score() : 0, buf);
		WRITE_SEP();
		o.writeChars("XS:i:");
		o.writeChars(buf);
	}
	if(print_xn_) {
		// XN:i: Number of ambiguous bases in the referenece
		itoa10<size_t>(res.refNs(), buf);
		WRITE_SEP();
		o.writeChars("XN:i:");
		o.writeChars(buf);
	}
	if(print_x0_) {
		// X0:i: Number of best hits
	}
	if(print_x1_) {
		// X1:i: Number of sub-optimal best hits
	}
	size_t num_mm = 0;
	size_t num_go = 0;
	size_t num_gx = 0;
	for(size_t i = 0; i < res.ned().size(); i++) {
		if(res.ned()[i].isMismatch()) {
			num_mm++;
		} else if(res.ned()[i].isReadGap()) {
			num_go++;
			num_gx++;
			while(i < res.ned().size()-1 &&
				  res.ned()[i+1].pos == res.ned()[i].pos &&
				  res.ned()[i+1].isReadGap())
			{
				i++;
				num_gx++;
			}
		} else if(res.ned()[i].isRefGap()) {
			num_go++;
			num_gx++;
			while(i < res.ned().size()-1 &&
				  res.ned()[i+1].pos == res.ned()[i].pos+1 &&
				  res.ned()[i+1].isRefGap())
			{
				i++;
				num_gx++;
			}
		}
	}
	if(print_xm_) {
		// XM:i: Number of mismatches in the alignment
		itoa10<size_t>(num_mm, buf);
		WRITE_SEP();
		o.writeChars("XM:i:");
		o.writeChars(buf);
	}
	if(print_xo_) {
		// XO:i: Number of gap opens
		itoa10<size_t>(num_go, buf);
		WRITE_SEP();
		o.writeChars("XO:i:");
		o.writeChars(buf);
	}
	if(print_xg_) {
		// XG:i: Number of gap extensions (incl. opens)
		itoa10<size_t>(num_gx, buf);
		WRITE_SEP();
		o.writeChars("XG:i:");
		o.writeChars(buf);
	}
	if(print_nm_) {
		// NM:i: Edit dist. to the ref, Ns count, clipping doesn't
		itoa10<size_t>(res.ned().size(), buf);
		WRITE_SEP();
		o.writeChars("NM:i:");
		o.writeChars(buf);
	}
	if(print_md_) {
		// MD:Z: String for mms. [0-9]+(([A-Z]|\^[A-Z]+)[0-9]+)*2
		WRITE_SEP();
		o.writeChars("MD:Z:");
		res.printMD(
			false,      // print colors
			exEnds,     // exclude nucleotide ends
			const_cast<EList<char>&>(tmpmdop_),    // MD operations
			const_cast<EList<char>&>(tmpmdch_),    // MD chars
			const_cast<EList<size_t>&>(tmpmdrun_), // MD run lengths
			&o,         // output buffer
			NULL);      // no char buffer
	}
	if(print_ys_ && summ.paired()) {
		// AS:i: Alignment score generated by aligner
		assert(res.oscore().valid());
		itoa10<TAlScore>(res.oscore().score(), buf);
		WRITE_SEP();
		o.writeChars("YS:i:");
		o.writeChars(buf);
	}
	if(print_yt_) {
		// YT:Z: String representing alignment type
		WRITE_SEP();
		flags.printYT(o);
	}
	if(print_yp_ && flags.partOfPair() && flags.canMax()) {
		// YP:i: Read was repetitive when aligned paired?
		WRITE_SEP();
		flags.printYP(o);
	}
	if(print_ym_ && flags.canMax() && (flags.isMixedMode() || !flags.partOfPair())) {
		// YM:i: Read was repetitive when aligned unpaired?
		WRITE_SEP();
		flags.printYM(o);
	}
	if(print_yf_ && flags.filtered()) {
		// YM:i: Read was repetitive when aligned unpaired?
		first = flags.printYF(o, first) && first;
	}
	if(print_yi_) {
		if(mapqInp[0] != '\0') {
			// YI:i: Suboptimal alignment score
			WRITE_SEP();
			o.writeChars("YI:Z:");
			o.writeChars(mapqInp);
		}
	}
	if(!rgs_.empty()) {
		WRITE_SEP();
		o.writeString(rgs_);
	}
}

/**
 * Print the optional flags to the given OutFileBuf.
 */
void SamConfig::printEmptyOptFlags(
	OutFileBuf& o,          // output buffer
	bool first,             // first opt flag printed is first overall?
	const AlnFlags& flags,  // alignment flags
	const AlnSetSumm& summ) // summary of alignments for this read
	const
{
	if(print_yt_) {
		// YT:Z: String representing alignment type
		WRITE_SEP();
		flags.printYT(o);
	}
	if(print_yp_ && flags.partOfPair() && flags.canMax()) {
		// YP:i: Read was repetitive when aligned paired?
		WRITE_SEP();
		flags.printYP(o);
	}
	if(print_ym_ && flags.canMax() && (flags.isMixedMode() || !flags.partOfPair())) {
		// YM:i: Read was repetitive when aligned unpaired?
		WRITE_SEP();
		flags.printYM(o);
	}
	if(print_yf_ && flags.filtered()) {
		// YM:i: Read was repetitive when aligned unpaired?
		first = flags.printYF(o, first) && first;
	}
	if(!rgs_.empty()) {
		WRITE_SEP();
		o.writeString(rgs_);
	}
}
