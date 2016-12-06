#ifndef SEMI_GLOBAL_ALIGN_H
#define SEMI_GLOBAL_ALIGN_H

#include <seqan/sequence.h>
#include <string>
#include <vector>
#include "kmers.h"
#include "commonkmerset.h"
#include "alignmentline.h"
#include "scoredalignment.h"
#include "random_alignments.h"
#include "string_functions.h"
#include "ref_seqs.h"
#include "nanoflann.hpp"

using namespace seqan;
using namespace nanoflann;


typedef std::pair<int, int> StartEndRange;
typedef std::unordered_map<std::string, std::vector<StartEndRange> > RefRangeMap;


// Functions that are called by the Python script must have C linkage, not C++ linkage.
extern "C" {

    char * semiGlobalAlignment(char * readNameC, char * readSeqC, int verbosity,
                               char * minimapAlignmentsStr, SeqMap * refSeqs,
                               int matchScore, int mismatchScore, int gapOpenScore,
                               int gapExtensionScore, double lowScoreThreshold, bool returnBad,
                               int sensitivityLevel);

//    char * startExtensionAlignment(char * read, char * ref,
//                                   int matchScore, int mismatchScore, int gapOpenScore,
//                                   int gapExtensionScore);
//
//    char * endExtensionAlignment(char * read, char * ref,
//                                 int matchScore, int mismatchScore, int gapOpenScore,
//                                 int gapExtensionScore);
}

std::vector<ScoredAlignment *> alignReadToReferenceRange(SeqMap * refSeqs, std::string refName,
                                                         StartEndRange refRange, int refLen,
                                                         std::string readName, char readStrand,
                                                         KmerPosMap * kmerPositions, int kSize,
                                                         std::string * readSeq, int matchScore,
                                                         int mismatchScore, int gapOpenScore,
                                                         int gapExtensionScore,
                                                         int sensitivityLevel,
                                                         int verbosity, std::string & output);

//ScoredAlignment * semiGlobalAlignmentOneLine(std::string & readName, std::string & refName,
//                                             std::string * readSeq, std::string * refSeq,
//                                             AlignmentLine * line, int verbosity, std::string & output,
//                                             Score<int, Simple> & scoringScheme);
//
//ScoredAlignment * semiGlobalAlignmentOneLineOneBand(std::string & readName, std::string & refName,
//                                                    Dna5String & readSeq, int readLen,
//                                                    Dna5String & refSeq, int refLen,
//                                                    AlignmentLine * line, int bandSize,
//                                                    int verbosity, std::string & output,
//                                                    Score<int, Simple> & scoringScheme);

double fractionOfReadAligned(std::vector<ScoredAlignment *> & alignments);

std::pair<int,int> getRefRange(int refStart, int refEnd, int refLen,
                               int readStart, int readEnd, int readLen, bool posStrand);

std::vector<std::pair<int, int> > simplifyRanges(std::vector<std::pair<int, int> > & ranges);

//CommonKmerSet * getHighestScoringSet(std::vector<CommonKmerSet *> & commonKmerSets);



// This stuff is for the nanoflann NN searching.
struct Point
{
    int x,y;
    bool operator==(const Point &other) const {return x == other.x && y == other.y;}
};

// http://stackoverflow.com/questions/17016175/c-unordered-map-using-a-custom-class-type-as-the-key#
namespace std {
  template <>
  struct hash<Point> {
    size_t operator()(const Point& p) const {
      return (std::hash<int>()(p.x) ^ (std::hash<int>()(p.y) << 1)) >> 1;
    }
  };
}


struct PointCloud
{
    std::vector<Point> pts;
    inline size_t kdtree_get_point_count() const { return pts.size(); }
    inline int kdtree_distance(const int *p1, const size_t idx_p2,size_t /*size*/) const
    {
        const int d0=p1[0]-pts[idx_p2].x;
        const int d1=p1[1]-pts[idx_p2].y;
        return d0+d1;
    }
    inline int kdtree_get_pt(const size_t idx, int dim) const
    {
        if (dim==0)
            return pts[idx].x;
        else
            return pts[idx].y;
    }
    template <class BBOX>
    bool kdtree_get_bbox(BBOX& /*bb*/) const { return false; }
};

typedef KDTreeSingleIndexAdaptor<L1_Adaptor<int, PointCloud>, PointCloud, 2> my_kd_tree_t;

std::vector<Point> radiusSearchAroundPoint(Point point, int radius, PointCloud & cloud,
                                           my_kd_tree_t & index);

std::vector<Point> getPointsInHighestDensityRegion(int searchRadius, std::string & trimmedRefSeq,
                                                   std::string * readSeq, PointCloud & cloud,
                                                   my_kd_tree_t & index);\

Point getHighestDensityPoint(int densityRadius, PointCloud & cloud, my_kd_tree_t & index,
                             std::string & trimmedRefSeq, std::string * readSeq,
                             double * highestDensityScore);

Point getHighestDensityPointNearPoint(int densityRadius, Point centre, PointCloud & cloud,
                                      my_kd_tree_t & index, double highestDensityScore,
                                      bool * failed);

double getPointDensityScore(int densityRadius, Point p, PointCloud & cloud, my_kd_tree_t & index);

void addKmerPointsToNanoflann(PointCloud & cloud, std::vector<CommonKmer> & commonKmers);


#endif // SEMI_GLOBAL_ALIGN_H
