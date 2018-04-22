// check validaty of passed in parameters
void checkValidity(int CWND, float ProbLoss, float ProbCorruption)
{
	if(ProbLoss >  1 || ProbLoss < 0)
	{
		fprintf(stderr,"ProbLoss should be between 0 and 1\n");
		exit(1);
	}
	if(CWND < 1 || CWND > 20)
	{
		fprintf(stderr,"CWND should be between 1 and 20\n");
		exit(1);
	}

	if(ProbCorruption > 1 || ProbCorruption < 0)
	{
		fprintf(stderr,"ProbCorruption should be between 0 and 1\n");
		exit(1);
	}
}

void checkCorrLossValidity(float ProbLoss, float ProbCorruption) 
{
	if(ProbCorruption > 1 || ProbCorruption < 0)
	{
		fprintf(stderr,"ProbCorruption should be between 0 and 1\n");
		exit(1);
	}
	if(ProbLoss > 1 || ProbLoss < 0)
	{
		fprintf(stderr,"ProbLoss should be between 0 and 1\n");
		exit(1);
	}
}