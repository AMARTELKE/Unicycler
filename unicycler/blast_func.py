"""
Functions relating to BLAST.

Author: Ryan Wick
email: rrwick@gmail.com
"""

import os
import subprocess
from .misc import load_fasta


class CannotFindStart(Exception):
    pass


def find_start_gene(sequence, start_genes_fasta, identity_threshold, coverage_threshold, blast_dir,
                    makeblastdb_path, tblastn_path, threads, verbosity):
    """
    This function uses tblastn to look for start genes in the sequence. It returns the first gene
    (using the order in the file) which meets the identity and coverage thresholds, as well as
    the position of that gene (including which strand it is on).
    This function assumes that the sequence is circular with no overlap.
    """
    # Prepare the replicon sequence. In order to get a solid, single BLAST hit in cases where the
    # gene overlaps from the end to the start, we have to duplicate some of the replicon sequence
    # for the BLAST database.
    seq_len = len(sequence)
    queries = load_fasta(start_genes_fasta)
    if not queries:
        raise CannotFindStart
    longest_query = max(len(x[1]) for x in queries)
    longest_query *= 3  # amino acids to nucleotides
    dup_length = min(seq_len, longest_query)
    sequence = sequence + sequence[:dup_length]

    # Create a FASTA file of the replicon sequence.
    replicon_fasta_filename = os.path.join(blast_dir, 'replicon.fasta')
    replicon_fasta = open(replicon_fasta_filename, 'w')
    replicon_fasta.write('>replicon\n')
    replicon_fasta.write(sequence)
    replicon_fasta.write('\n')
    replicon_fasta.close()

    # Build the BLAST database.
    command = [makeblastdb_path, '-dbtype', 'nucl', '-in', replicon_fasta_filename]
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    _, err = process.communicate()
    if err:
        print('\nmakeblastdb encountered an error:\n' + err.decode())
        raise CannotFindStart

    # Run the tblastn search.
    command = [tblastn_path, '-db', replicon_fasta_filename, '-query', start_genes_fasta, '-outfmt',
               '6 qseqid sstart send pident qlen qseq qstart bitscore', '-num_threads',
               str(threads)]
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    blast_out, blast_err = process.communicate()
    process.wait()
    if blast_err and verbosity > 1:
        print('\nBLAST encountered an error:\n' + blast_err.decode())

    # Find the best hit in the results.
    best_hit, best_bitscore = None, 0
    for line in blast_out.decode().splitlines():
        hit = BlastHit(line, seq_len)
        if hit.pident >= identity_threshold and hit.query_cov >= coverage_threshold and \
                hit.qstart == 0 and hit.bitscore > best_bitscore:
            best_hit = hit
            best_bitscore = hit.bitscore

    if best_bitscore:
        return best_hit
    else:
        raise CannotFindStart


class BlastHit(object):
    def __init__(self, blast_line, seq_len):

        self.qseqid = ''
        self.pident, self.qstart, self.bitscore, self.query_cov, self.start_pos = 0, 0, 0, 0, 0
        self.flip = False

        parts = blast_line.strip().split('\t')
        if len(parts) > 7:
            self.qseqid = parts[0]
            self.pident = float(parts[3])
            self.qstart = int(parts[6]) - 1
            self.bitscore = float(parts[7])

            sstart = int(parts[1]) - 1
            send = int(parts[2])
            qlen = float(parts[4])
            qseq = parts[5]
            self.query_cov = 100.0 * len(qseq) / qlen

            if sstart <= send:
                self.start_pos = sstart
                self.flip = False
            else:
                self.start_pos = sstart + 1
                self.flip = True
            if self.start_pos >= seq_len:
                self.start_pos -= seq_len
