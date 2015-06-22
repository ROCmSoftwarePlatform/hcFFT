#include "ampfftlib.h"

/*----------------------------------------------------FFTPlan-----------------------------------------------------------------------------*/
ampfftStatus FFTPlan::ampfftCreateDefaultPlan (ampfftPlanHandle* plHandle,ampfftDim dimension, const size_t *length)
{
  return AMPFFT_SUCCESS;
}

ampfftStatus FFTPlan::executePlan(FFTPlan* fftPlan)
{
  if(!fftPlan)
    return AMPFFT_INVALID;

  return AMPFFT_SUCCESS;
}
/*----------------------------------------------------FFTPlan-----------------------------------------------------------------------------*/

/*---------------------------------------------------FFTRepo--------------------------------------------------------------------------------*/
ampfftStatus FFTRepo::createPlan( ampfftPlanHandle* plHandle, FFTPlan*& fftPlan )
{
	scopedLock sLock( lockRepo, _T( "insertPlan" ) );

	//	We keep track of this memory in our own collection class, to make sure it's freed in releaseResources
	//	The lifetime of a plan is tracked by the client and is freed when the client calls ::ampfftDestroyPlan()
	fftPlan	= new FFTPlan;

	//	We allocate a new lock here, and expect it to be freed in ::ampfftDestroyPlan();
	//	The lifetime of the lock is the same as the lifetime of the plan
	lockRAII* lockPlan	= new lockRAII;

	//	Add and remember the fftPlan in our map
	repoPlans[ planCount ] = make_pair( fftPlan, lockPlan );

	//	Assign the user handle the plan count (unique identifier), and bump the count for the next plan
	*plHandle	= planCount++;

	return	AMPFFT_SUCCESS;
}


ampfftStatus FFTRepo::getPlan( ampfftPlanHandle plHandle, FFTPlan*& fftPlan, lockRAII*& planLock )
{
	scopedLock sLock( lockRepo, _T( "getPlan" ) );

	//	First, check if we have already created a plan with this exact same FFTPlan
	repoPlansType::iterator iter	= repoPlans.find( plHandle );
	if( iter == repoPlans.end( ) )
		return	AMPFFT_ERROR;

	//	If plan is valid, return fill out the output pointers
	fftPlan		= iter->second.first;
	planLock	= iter->second.second;

	return	AMPFFT_SUCCESS;
}

ampfftStatus FFTRepo::deletePlan( ampfftPlanHandle* plHandle )
{
	scopedLock sLock( lockRepo, _T( "deletePlan" ) );

	//	First, check if we have already created a plan with this exact same FFTPlan
	repoPlansType::iterator iter	= repoPlans.find( *plHandle );
	if( iter == repoPlans.end( ) )
		return	AMPFFT_ERROR;

	//	We lock the plan object while we are in the process of deleting it
	{
		scopedLock sLock( *iter->second.second, _T( "ampfftDestroyPlan" ) );

		//	Delete the FFTPlan
		delete iter->second.first;
	}

		//	Delete the lockRAII
	delete iter->second.second;

	//	Remove entry from our map object
	repoPlans.erase( iter );

	//	Clear the client's handle to signify that the plan is gone
	*plHandle = 0;

	return	AMPFFT_SUCCESS;
}

ampfftStatus FFTRepo::setProgramEntryPoints( const ampfftGenerators gen, const ampfftPlanHandle& handle,
                                             const FFTKernelGenKeyParams& fftParam, const char * kernel_fwd,
                                             const char * kernel_back)
{
	scopedLock sLock( lockRepo, _T( "setProgramEntryPoints" ) );

	fftRepoKey key = std::make_pair( gen, handle );

	fftRepoValue& fft = mapFFTs[ key ];
	fft.EntryPoint_fwd  = kernel_fwd;
	fft.EntryPoint_back = kernel_back;

	return	AMPFFT_SUCCESS;
}

ampfftStatus FFTRepo::getProgramEntryPoint( const ampfftGenerators gen, const ampfftPlanHandle& handle,
                                            const FFTKernelGenKeyParams& fftParam, ampfftDirection dir,
                                            std::string& kernel)
{
	scopedLock sLock( lockRepo, _T( "getProgramEntryPoint" ) );

	fftRepoKey key = std::make_pair( gen, handle );

	fftRepo_iterator pos = mapFFTs.find( key );

	if( pos == mapFFTs.end( ) )
		return	AMPFFT_ERROR;

	switch (dir) {
	case AMPFFT_FORWARD:
		kernel = pos->second.EntryPoint_fwd;
		break;
	case AMPFFT_BACKWARD:
		kernel = pos->second.EntryPoint_back;
		break;
	default:
		assert (false);
		return AMPFFT_ERROR;
	}

	if (0 == kernel.size())
		return	AMPFFT_ERROR;

	return	AMPFFT_SUCCESS;
}

ampfftStatus FFTRepo::setProgramCode( const ampfftGenerators gen, const ampfftPlanHandle& handle, const FFTKernelGenKeyParams& fftParam, const std::string& kernel)
{
	scopedLock sLock( lockRepo, _T( "setProgramCode" ) );

	fftRepoKey key = std::make_pair( gen, handle );

	// Prefix copyright statement at the top of generated kernels
	std::stringstream ss;
	ss <<
		"/* ************************************************************************\n"
		" * Copyright 2013 MCW, Inc.\n"
		" *\n"
		" * ************************************************************************/"
	<< std::endl << std::endl;

	std::string prefixCopyright = ss.str();

	mapFFTs[ key ].ProgramString = prefixCopyright + kernel;

	return	AMPFFT_SUCCESS;
}

ampfftStatus FFTRepo::getProgramCode( const ampfftGenerators gen, const ampfftPlanHandle& handle, const FFTKernelGenKeyParams& fftParam, std::string& kernel)
{

	scopedLock sLock( lockRepo, _T( "getProgramCode" ) );
	fftRepoKey key = std::make_pair( gen, handle );

	fftRepo_iterator pos = mapFFTs.find( key);
	if( pos == mapFFTs.end( ) )
		return	AMPFFT_ERROR;

        kernel = pos->second.ProgramString;
	return	AMPFFT_SUCCESS;
}

ampfftStatus FFTRepo::releaseResources( )
{
	scopedLock sLock( lockRepo, _T( "releaseResources" ) );

	//	Free all memory allocated in the repoPlans; represents cached plans that were not destroyed by the client
	//
	for( repoPlansType::iterator iter = repoPlans.begin( ); iter != repoPlans.end( ); ++iter )
	{
		FFTPlan* plan	= iter->second.first;
		lockRAII* lock	= iter->second.second;
		if( plan != NULL )
		{
			delete plan;
		}
		if( lock != NULL )
		{
			delete lock;
		}
	}

	//	Reset the plan count to zero because we are guaranteed to have destroyed all plans
	planCount	= 1;

	//	Release all strings
	mapFFTs.clear( );

	return	AMPFFT_SUCCESS;
}
/*------------------------------------------------FFTRepo----------------------------------------------------------------------------------*/
