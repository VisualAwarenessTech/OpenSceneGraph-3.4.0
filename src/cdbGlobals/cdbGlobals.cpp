// Copyright (c) 2016-2017 Visual Awareness Technologies and Consulting Inc, St Petersburg FL
// This file is based on the Common Database (CDB) Specification for USSOCOM
// Version 3.0 – October 2008

// CDB_Tile is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// CDB_Tile is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.

// You should have received a copy of the GNU Lesser General Public License
// along with CDB_Tile.  If not, see <http://www.gnu.org/licenses/>.

// 2015 GAJ Geospatial Enterprises, Orlando FL
// Modified for General Incorporation of Common Database (CDB) support within osgEarth
//
#include <cdbGlobals/cdbGlobals>
#include <osgDB/FileUtils>
#include <osgDB/FileNameUtils>

#ifdef _WIN32
#include <Windows.h>
#endif

CDB_Global CDB_Global_Instance;

CDB_Global::CDB_Global() : m_ogrDataset(NULL)
{
	int s_BaseMapLodNum = 0;
	bool s_EnableBathymetry = true;
	bool s_LOD0_GS_FullStack = false;
	bool s_LOD0_GT_FullStack = false;
	bool s_CDB_Tile_Be_Verbose = false;
}

CDB_Global::~CDB_Global()
{
	if (m_ogrDataset)
	{
		GDALClose(m_ogrDataset);
		m_ogrDataset = NULL;
	}
}

bool CDB_Global::Open_Vector_File(std::string FileName)
{
	bool FileExists = osgDB::fileExists(FileName);
	if (FileExists && (FileName.find(".gpkg") != std::string::npos))
	{
		//		GDALOpenInfo oOpenInfoP(m_FileName.c_str(), GA_ReadOnly | GDAL_OF_VECTOR);
		//		m_GDAL.poDataset = m_GDAL.poDriver->pfnOpen(&oOpenInfoP);
		char * drivers[2];
		drivers[0] = "GPKG";
		drivers[1] = NULL;
		m_ogrDataset = (GDALDataset *)GDALOpenEx(FileName.c_str(), GDAL_OF_VECTOR | GA_ReadOnly | GDAL_OF_SHARED, drivers, NULL, NULL);
		if (!m_ogrDataset)
			return false;
	}
	else
		return false;
	return true;
}

bool CDB_Global::Has_Layer(std::string LayerName)
{
	if (!m_ogrDataset)
		return false;
	else
	{
		OGRLayer * poLayer = m_ogrDataset->GetLayerByName(LayerName.c_str());
		if (poLayer)
			return true;
		else
			return false;
	}
}

GDALDataset * CDB_Global::Get_Dataset(void)
{
	return m_ogrDataset;
}

void CDB_Global::Set_BaseMapLodNum(int num)
{
	s_BaseMapLodNum = num;
}

int CDB_Global::BaseMapLodNum(void)
{
	return s_BaseMapLodNum;
}

void CDB_Global::Set_EnableBathymetry(bool value)
{
	s_EnableBathymetry = value;
}

bool CDB_Global::EnableBathymetry(void)
{
	return s_EnableBathymetry;
}

void CDB_Global::Set_LOD0_GS_FullStack(bool value)
{
	s_LOD0_GS_FullStack = value;
}

bool CDB_Global::LOD0_GS_FullStack(void)
{
	return s_LOD0_GS_FullStack;
}

void CDB_Global::Set_LOD0_GT_FullStack(bool value)
{
	s_LOD0_GT_FullStack = value;
}

bool CDB_Global::LOD0_GT_FullStack(void)
{
	return s_LOD0_GT_FullStack;
}

void CDB_Global::Set_CDB_Tile_Be_Verbose(bool value)
{
	s_CDB_Tile_Be_Verbose = value;
}

bool CDB_Global::CDB_Tile_Be_Verbose(void)
{
	return s_CDB_Tile_Be_Verbose;
}

CDB_Global * CDB_Global::getInstance(void)
{
	return &CDB_Global_Instance;
}

