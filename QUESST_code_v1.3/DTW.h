
//a C++ version of sub-sequence DTW, which do the length normalization on the fly

#include <infra.h>
#include <math.h>
#include <string>

#define LEN_PENALTY_DIAG 2

// unterance-level mean variance normalization

// smooth the distance matrix
void smooth(const infra::matrix& dist, infra::matrix& sdist, int context = 1)
{

	unsigned long nHeight = dist.height();
	unsigned long nWidth = dist.width();

	int length = 2*context + 1;

	for (int i = 0; i < nHeight; i++)
		for(int j = 0; j < nWidth; j++)
		{
			float sum = dist(i,j); 
			for (int k=1; k <= context; k++)
			{
				int ii = std::max(0,i-k);
				int jj = std::max(0,j-k);
				sum+=dist(ii,jj);
				ii = std::min(int(nHeight-1),i+k);
				jj = std::min(int(nWidth-1),j+k);
				sum+=dist(ii,jj);
			}
			sdist(i,j) = sum/length;
		}


}
//normalize the feature data , make the every line's sum square equal to 1
void normalizeFea(infra::matrix& feature)
{
	unsigned long nHeight = feature.height();
	unsigned long nWidth = feature.width();

	for(int i = 0; i < nHeight; i++)
	{
		float sum=0;
		for(int j = 0; j < nWidth; j++)
			sum += feature(i,j)*feature(i,j);

	    float mod = sqrt(sum);	
		for(int j = 0; j < nWidth; j++)
	        feature(i,j) /= mod;	
	}

}
//every column minus the mean and divide the viarance 
void mvn(infra::matrix& feature)
{
	unsigned long nHeight = feature.height();
	unsigned long nWidth = feature.width();

	for(int i = 0; i < nWidth; i++)
	{
		float mean = feature.column(i).sum()/nHeight;	
		feature.column(i) -= mean; // centralize
	    float std  = sqrt((feature.column(i)*feature.column(i))/nHeight);
		feature.column(i) /= std;  
			
	}

}

double KLDivergenceDistance(const infra::vector& vect_a, const infra::vector& vect_b) {
    infra::vector log_vect_a(vect_a.size());
    infra::vector log_vect_b(vect_b.size());
    log_vect_a.zeros();
    log_vect_a += vect_a;
    log_vect_a.log();
    log_vect_b.zeros();
    log_vect_b +=vect_b;
    log_vect_b.log();

    double res1 = (vect_a * log_vect_a) - (vect_a * log_vect_b);
    double res2 = (vect_b * log_vect_b) - (vect_b * log_vect_a);

    return 0.5*(res1+res2);
}

//calculate the frame distance of one query to one testfile 
//input：query matrix
//       testfile matrix
//output:distdence matrix (query_framenum*test_framenum)
 
void computeDist( const infra::matrix& query, const infra::matrix& test, infra::matrix& dist_matrix, std::string featureType)
{
	unsigned long height = dist_matrix.height();
	unsigned long width = dist_matrix.width();
	unsigned long dim = query.width();

	if(featureType == "mfcc" || featureType == "isa"|| featureType == "lpp" || featureType == "vtlnmfc" || featureType.find("sbnf")!=std::string::npos)
	{
		infra::prod_t(query,test,dist_matrix); // dist_matrix = query*test.T
		dist_matrix = 1 - dist_matrix;
	}
	else if(featureType.find("pg")!=std::string::npos||featureType=="vtln_k_2048_gpg"||featureType == "enpg" || featureType =="ruspg"||featureType == "czpg"||featureType == "hupg") // "pg" denote any kind of posteriorgram.
	{
	    infra::matrix matrix_query_log(height, dim);
        infra::matrix matrix_test_log(width, dim);
        infra::vector vector_query_log_query(height);
        infra::vector vector_test_log_test(width);
        infra::matrix matrix_query_log_test(height, width);
        infra::matrix matrix_test_log_query(width, height);
        infra::matrix matrix_test_log_query_T(height, width);
        infra::matrix matrix_eye(height, height);
        matrix_eye.eye();

        matrix_query_log.zeros();
        matrix_query_log = matrix_query_log + query;
        matrix_query_log.log();

        matrix_test_log.zeros();
        matrix_test_log = matrix_test_log + test;
        matrix_test_log.log();

        for (int i=0; i< height; i++) {
            vector_query_log_query(i) = query.row(i) * matrix_query_log.row(i);
        }

        for (int i=0; i<width; i++) {
            vector_test_log_test(i) = test.row(i) * matrix_test_log.row(i);
        }

        infra::prod_t(query, matrix_test_log, matrix_query_log_test);
        infra::prod_t(test, matrix_query_log,matrix_test_log_query);

        matrix_query_log_test = 0 - matrix_query_log_test;
        matrix_test_log_query = 0 - matrix_test_log_query;
        infra::add_column_vector(matrix_query_log_test, vector_query_log_query);
        infra::add_column_vector(matrix_test_log_query, vector_test_log_test);
        infra::prod_t(matrix_eye,  matrix_test_log_query, matrix_test_log_query_T);
        infra::sum(matrix_test_log_query_T, matrix_query_log_test, dist_matrix);
        dist_matrix = 0.5 * dist_matrix;


    }
	else
	{
		std::cout<<"This type of feature representation is not supported"<<std::endl;
		exit(1);
	}
}

//function: subsequeceDTW
//discription: calculate score of a query match a test file using subsequeceDTW method
//input: frame distance Matrix of one query to one testfile 
float subsequnceDTW(const infra::matrix& dist)
{
	unsigned long nHeight = dist.height();
	unsigned long nWidth = dist.width();
	infra::matrix avg_cost(nHeight,nWidth);
	infra::matrix cost(nHeight,nWidth);
	infra::matrix trace(nHeight,nWidth);
	infra::matrix length(nHeight,nWidth);
	
	// initialize
	// trace(i,j) = 0 denote the precedent point of (i,j) is (i-1,j)
	// trace(i,j) = 1                                        (i,j-1) 
	// trace(i,j) = 2                                        (i-1,j-1)
	
	for( int i = 0; i < nWidth; i++)
	{
		avg_cost(0,i) = dist(0,i);
		cost(0,i) = dist(0,i);
		trace(0,i) = 0;
		length(0,i) = 1;
	}

	for( int i = 1; i < nHeight; i++)
	{
		trace(i,0) = 0;
		length(i,0) = i+1;
		cost(i,0) = dist(i,0)+cost(i-1,0);
		avg_cost(i,0)= cost(i,0)/length(i,0);
	}

	// fill the three matrices in a dynamic programming style.
	for( int i = 1; i < nHeight; i++)
		for( int j = 1; j < nWidth; j++)
		{
		    // compute the three possible costs
			double cost_0 = dist(i,j)+cost(i-1,j);
			double cost_1 = dist(i,j)+cost(i,j-1);
			double cost_2 = dist(i,j)+cost(i-1,j-1);
			double avg_cost_0 = cost_0/(1+length(i-1,j));
			double avg_cost_1 = cost_1/(1+length(i,j-1));
			double avg_cost_2 = cost_2/(LEN_PENALTY_DIAG+length(i-1,j-1));
			
			// choose the one which lead to the minimum cost as the precedent point 
			if(avg_cost_0 < avg_cost_1)
			{
				if(avg_cost_0 < avg_cost_2)
				{
					avg_cost(i,j) = avg_cost_0;
					cost(i,j) = cost_0;
					length(i,j) = 1 + length(i-1,j);
					trace(i,j) = 0;
				}
				else
				{
					avg_cost(i,j) = avg_cost_2;
					cost(i,j) = cost_2;
					length(i,j) = LEN_PENALTY_DIAG + length(i-1,j-1);
					trace(i,j) = 2;
				}
			}
			else if(avg_cost_1 < avg_cost_2)
			{
				avg_cost(i,j) = avg_cost_1;
				cost(i,j) = cost_1;
				length(i,j) = 1 + length(i,j-1);
				trace(i,j) = 1;
			}
			else
			{
				avg_cost(i,j) = avg_cost_2;
				cost(i,j) = cost_2;
				length(i,j) = LEN_PENALTY_DIAG + length(i-1,j-1);
				trace(i,j) = 2;
 			}
				
		}

	float min_cost = avg_cost.row( nHeight - 1 ).min();
	return 1 - min_cost;
}













