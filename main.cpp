//#define MC 1.819444444444444e-5//3.275/180000

//#define MC (3.14113849765258e-6 / 0.00709375)
//#define MC 0.0002819383
#define MC 3.14113849765258e-6 / 0.035

#include <gdal.h>
#include "cpl_string.h"
#include <ogr_spatialref.h>
#include <gdal_priv.h>

#define PARAMS_SIZE 5

/*
 * Initial orientation using GPS data
*/

int main(int argc, char* argv[])
{
	GDALAllRegister();

    const char* soMetadataPath = "/home/andrew/workspace/GPS_assembling/Debug/gps.txt";
    printf("I'm alive");

    char **papszLines = CSLLoad( soMetadataPath );
    int nLinesCount = CSLCount(papszLines);

    //const char* err = CPLGetLastErrorMsg();

    if( nLinesCount == 0 )
    {
        printf("File is empty");
        return false;
    }

    OGRSpatialReference oOGRSpatialReference(SRS_WKT_WGS84);
    OGRSpatialReference oDstOGRSpatialReference(SRS_WKT_WGS84);

    const char* delim = ", ";
    for( int i = 0; papszLines[i] != NULL; ++i )
    {
        char **papszLineItems = CSLTokenizeStringComplex(papszLines[i], delim, FALSE, FALSE);
        if(papszLineItems == NULL )
            continue;

        char* tmp = papszLines[i];
        int params = CSLCount(papszLineItems);


		//Get file name
		char* fileName = papszLineItems[0];

		if(CPLCheckForFile(fileName, NULL))
		{
			GDALDataset* poDataset = (GDALDataset*) GDALOpen(fileName, GA_ReadOnly );

			if(poDataset)
			{
				//Create wld file
				double dfX = CPLAtof(papszLineItems[2]);
				double dfY = CPLAtof(papszLineItems[1]);
				double dfH = CPLAtof(papszLineItems[3]);

				//From degrees to radians
				double dfAz = (360.0 - CPLAtof(papszLineItems[4])) * (M_PI / 180);
				double dfCos = cos(dfAz);
				double dfSin = sin(dfAz);
				//
				int nXSize = poDataset->GetRasterXSize();
				int nYSize = poDataset->GetRasterYSize();
				double dfPixSize = dfH * MC;
				double dfPixSizeH = dfPixSize / 2;
				double dfWidth = dfPixSizeH * nXSize;
				double dfLength = dfPixSizeH * nYSize;

				int nZoneNo = ceil(( 180.0 + dfX ) / 6.0);
				oDstOGRSpatialReference.SetUTM(nZoneNo, dfY > 0);

				OGRCoordinateTransformation *poCT = OGRCreateCoordinateTransformation(
						&oOGRSpatialReference, &oDstOGRSpatialReference);

				if(poCT)
				{
					int nResult = poCT->Transform(1, &dfX, &dfY, NULL);
					if(nResult)
					{
						GDAL_GCP *pasGCPList = (GDAL_GCP *) CPLCalloc(sizeof(GDAL_GCP), 4);
						GDALInitGCPs( 4, pasGCPList );

						char *pszProjection = NULL;
						oDstOGRSpatialReference.morphToESRI();
						if( oDstOGRSpatialReference.exportToWkt( &pszProjection ) == OGRERR_NONE )
						{
							//write to  *.prj file
							const char  *pszT;
							VSILFILE    *fpT;

							pszT = CPLResetExtension(fileName, "prj");
							fpT = VSIFOpenL(pszT, "wt");
							if (fpT != NULL)
							{
								VSIFWriteL((void *)pszProjection, 1, CPLStrnlen( pszProjection, 2048 ), fpT );
								VSIFCloseL(fpT);
								CPLFree(pszProjection);
							}
						}
						oDstOGRSpatialReference.morphFromESRI();

						double dfXnew1 = dfX + (dfWidth * dfCos) - (dfLength * dfSin);     //URX
						double dfYnew1 = dfY + (dfWidth * dfSin) + (dfLength * dfCos);    //URY
						double dfXnew2 = dfX + (dfWidth * dfCos) - (-dfLength * dfSin);     //LRX
						double dfYnew2 = dfY + (dfWidth * dfSin) + (-dfLength * dfCos);    //LRY
						double dfXnew3 = dfX + (-dfWidth * dfCos) - (-dfLength * dfSin);     //LLX
						double dfYnew3 = dfY + (-dfWidth * dfSin) + (-dfLength * dfCos);    //LLY
						double dfXnew4 = dfX + (-dfWidth * dfCos) - (dfLength * dfSin);     //ULX
						double dfYnew4 = dfY + (-dfWidth * dfSin) + (dfLength * dfCos);    //ULY

						pasGCPList[0].pszId = CPLStrdup( "1" );
						pasGCPList[0].dfGCPX = dfXnew4;
						pasGCPList[0].dfGCPY = dfYnew4;
						pasGCPList[0].dfGCPZ = 0;
						pasGCPList[0].dfGCPLine = 0.5;
						pasGCPList[0].dfGCPPixel = 0.5;

						pasGCPList[1].pszId = CPLStrdup( "2" );
						pasGCPList[1].dfGCPX = dfXnew1;
						pasGCPList[1].dfGCPY = dfYnew1;
						pasGCPList[1].dfGCPZ = 0;
						pasGCPList[1].dfGCPLine = 0.5;
						pasGCPList[1].dfGCPPixel = nXSize - 0.5;

						pasGCPList[2].pszId = CPLStrdup( "3" );
						pasGCPList[2].dfGCPX = dfXnew2;
						pasGCPList[2].dfGCPY = dfYnew2;
						pasGCPList[2].dfGCPZ = 0;
						pasGCPList[2].dfGCPLine = nYSize - 0.5;
						pasGCPList[2].dfGCPPixel = nXSize - 0.5;

						pasGCPList[3].pszId = CPLStrdup( "4" );
						pasGCPList[3].dfGCPX = dfXnew3;
						pasGCPList[3].dfGCPY = dfYnew3;
						pasGCPList[3].dfGCPZ = 0;
						pasGCPList[3].dfGCPLine = nYSize - 0.5;
						pasGCPList[3].dfGCPPixel = 0.5;

						double adfGeoTransform[6] = { 0, 0, 0, 0, 0, 0 };
						nResult = GDALGCPsToGeoTransform(4, pasGCPList, adfGeoTransform, 1);
						if(nResult)
						{
							//Write world file
							nResult = GDALWriteWorldFile(fileName, "jgw", adfGeoTransform);
						}
					}
					OCTDestroyCoordinateTransformation(poCT);
				}
				GDALClose(poDataset);
			}
		}

        CSLDestroy( papszLineItems );
    }

    CSLDestroy( papszLines );
}
