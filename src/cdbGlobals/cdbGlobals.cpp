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

#pragma warning (push)
#pragma warning (disable : 4251)
#include <cdbGlobals/cdbGlobals>
#include <gdal_priv.h>
#include <ogr_geopackage.h>

#include <osgDB/FileUtils>
#include <osgDB/FileNameUtils>
#include <ios>

#ifdef _WIN32
#include <Windows.h>
#endif
#pragma warning (pop)

CDB_Global CDB_Global_Instance;

CDB_Global::CDB_Global() : m_ogrDataset(NULL)
{
	int s_BaseMapLodNum = 0;
	bool s_EnableBathymetry = true;
	bool s_LOD0_GS_FullStack = false;
	bool s_LOD0_GT_FullStack = false;
	bool s_CDB_Tile_Be_Verbose = false;
	m_doneList.clear();
}

CDB_Global::~CDB_Global()
{
	if (m_ogrDataset)
	{
		GDALClose(m_ogrDataset);
		m_ogrDataset = NULL;
	}

	for (std::map<std::string, GSMediaMemory>::iterator id = m_GSGeometryMap.begin(); id != m_GSGeometryMap.end(); ++id)
	{
		GSMediaMemory Mem = id->second;
		if (Mem.bufferdata)
		{
			delete Mem.bufferdata;
			Mem.bufferdata = NULL;
			Mem.bufsize = 0;
		}
	}

	for (std::map<std::string, GSMediaMemory>::iterator id = m_GSTextureMap.begin(); id != m_GSTextureMap.end(); ++id)
	{
		GSMediaMemory Mem = id->second;
		if (Mem.bufferdata)
		{
			delete Mem.bufferdata;
			Mem.bufferdata = NULL;
			Mem.bufsize = 0;
		}
	}

	if (m_GTGeometry.bufferdata)
	{
		delete m_GTGeometry.bufferdata;
		m_GTGeometry.bufferdata = NULL;
		m_GTGeometry.bufsize = 0;
	}

	if (m_GTTexture.bufferdata)
	{
		delete m_GTTexture.bufferdata;
		m_GTTexture.bufferdata = NULL;
		m_GTGeometry.bufsize = 0;
	}

}

void CDB_Global::Check_Done(void)
{
	for each (std::string stringkey in m_doneList)
	{
		std::map<std::string, GSMediaMemory>::iterator id = m_GSGeometryMap.find(stringkey);
		if (id != m_GSGeometryMap.end())
		{
			GSMediaMemory myMem = m_GSGeometryMap[stringkey];
			delete myMem.bufferdata;
			myMem.bufferdata = NULL;
			m_GSGeometryMap.erase(id);
		}

		std::map<std::string, GSMediaMemory>::iterator tid = m_GSTextureMap.find(stringkey);
		if (tid != m_GSTextureMap.end())
		{
			GSMediaMemory myMem = m_GSTextureMap[stringkey];
			delete myMem.bufferdata;
			myMem.bufferdata = NULL;
			m_GSTextureMap.erase(tid);
		}
	}
	m_doneList.clear();
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
		m_ogrDataset = (GDALDataset *) GDALOpenEx(FileName.c_str(), GDAL_OF_VECTOR | GA_ReadOnly | GDAL_OF_SHARED, drivers, NULL, NULL);
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


bool CDB_Global::Load_Media(const std::string MediaId)
{
	GSMediaKey MyKey;
	std::string gpkgTableName;
	GSTableType TableType = GSInvalid;
	if (!ParseGSKey(MediaId, MyKey, gpkgTableName, TableType))
		return false;
	return Add_Media_to_Map(MyKey, gpkgTableName, TableType);
}


bool CDB_Global::Add_Media_to_Map(GSMediaKey MyKey, std::string gpkgTableName, GSTableType TableType)
{

	std::string stringkey = MyKey.ToString();
	if (TableType == GSGeometry)
	{
		
		std::map<std::string, GSMediaMemory>::iterator id = m_GSGeometryMap.find(stringkey);
		if (id != m_GSGeometryMap.end())
			return true;
	}
	else if (TableType == GSTexture)
	{
		std::map<std::string, GSMediaMemory>::iterator id = m_GSTextureMap.find(stringkey);
		if (id != m_GSTextureMap.end())
			return true;
	}
	else if (TableType == GTGeometry)
	{
		if (m_GTGeometry.bufferdata != NULL)
			return true;
	}
	else if (TableType == GTTexture)
	{
		if (m_GTTexture.bufferdata != NULL)
			return true;
	}
	else
	{
		return false;
	}


	GDALGeoPackageDataset * poDataset = (GDALGeoPackageDataset *)m_ogrDataset;
	sqlite3 *hDB = poDataset->GetDB();

	GSMediaMemory fin;
	char* pszSelectMedia = sqlite3_mprintf("SELECT data FROM \"%w\" WHERE uref = ? AND rref = ? ", gpkgTableName.c_str());
	sqlite3_stmt *stmt;
	int rc;
	rc = sqlite3_prepare_v2(hDB, pszSelectMedia, -1, &stmt, NULL);
	if (rc == SQLITE_OK)
	{
		sqlite3_bind_int(stmt, 1, MyKey.UrefNum);
		sqlite3_bind_int(stmt, 2, MyKey.RrefNum);
		rc = sqlite3_step(stmt);
		if (rc == SQLITE_ROW)
		{
			fin.bufsize = sqlite3_column_bytes(stmt, 0);
			fin.bufferdata = new char[fin.bufsize];
			std::memcpy(fin.bufferdata, (char *)sqlite3_column_blob(stmt, 0), fin.bufsize);
			rc = sqlite3_finalize(stmt);
			if (rc == SQLITE_SCHEMA)
				return false;

			if (TableType == GSGeometry)
			{
				m_GSGeometryMap.insert(std::pair<std::string, GSMediaMemory>(stringkey, fin));
			}
			else if (TableType == GSTexture)
			{
				m_GSTextureMap.insert(std::pair<std::string, GSMediaMemory>(stringkey, fin));
			}
			else if (TableType == GTGeometry)
			{
				m_GTGeometry = fin;
			}
			else if (TableType == GTTexture)
			{
				m_GTTexture = fin;
			}
		}
		else
			return false;
	}
	else
		return false;

	return true;
}

bool CDB_Global::Get_Media(const std::string &mediaId, std::stringstream &fin)
{
	GSMediaKey MyKey;
	std::string gpkgTableName;
	GSTableType TableType = GSInvalid;
	if (!ParseGSKey(mediaId, MyKey, gpkgTableName, TableType))
		return false;
#ifdef _DEBUG
	int fubar = 0;
#endif
	std::string stringkey = MyKey.ToString();
	if (TableType == GSGeometry)
	{

		std::map<std::string, GSMediaMemory>::iterator id = m_GSGeometryMap.find(stringkey);
		if (id != m_GSGeometryMap.end())
		{
			GSMediaMemory myMem = m_GSGeometryMap[stringkey];
			if (!fin.write(myMem.bufferdata, myMem.bufsize))
			{
#ifdef _DEBUG
				++fubar;
#endif
			}
			fin.seekg(0, std::ios::beg);
		}
		else
		{
			return false;
		}
	}
	else if (TableType == GSTexture)
	{
		std::map<std::string, GSMediaMemory>::iterator id = m_GSTextureMap.find(stringkey);
		if (id != m_GSTextureMap.end())
		{
			GSMediaMemory myMem = m_GSTextureMap[stringkey];
			if (!fin.write(myMem.bufferdata, myMem.bufsize))
			{
#ifdef _DEBUG
				++fubar;
#endif
			}
			fin.seekg(0, std::ios::beg);
			m_doneList.push_back(stringkey);
		}
		else
			return false;
	}
	else if (TableType == GTGeometry)
	{
		if (m_GTGeometry.bufferdata)
		{
			if (!fin.write(m_GTGeometry.bufferdata, m_GTGeometry.bufsize))
			{
#ifdef _DEBUG
				++fubar;
#endif
			}
			fin.seekg(0, std::ios::beg);

		}
	}
	else if (TableType == GTTexture)
	{
		if (m_GTTexture.bufferdata)
		{
			if (!fin.write(m_GTTexture.bufferdata, m_GTTexture.bufsize))
			{
#ifdef _DEBUG
				++fubar;
#endif
			}
			fin.seekg(0, std::ios::beg);

		}
	}
	else
	{
		return false;
	}

	return true;
}

#if 0
bool CDB_Global::Get_Media(const std::string &mediaId, std::ifstream &fin)
{
	GDALGeoPackageDataset * poDataset = (GDALGeoPackageDataset *)m_ogrDataset;
	sqlite3 *hDB = poDataset->GetDB();

	if (mediaId.substr(0, 5) != "gpkg:")
		return false;
	std::string tablename = mediaId.substr(5);
	size_t pos = tablename.find_first_of(":");
	if (pos == std::string::npos)
		return false;
	std::string refstring = tablename.substr(pos+1);
	tablename = tablename.substr(0, pos);


	pos = refstring.find_first_of(":");
	if (pos == std::string::npos)
		return false;

	std::string urefstr = refstring.substr(0, pos);
	std::string rrefstr = refstring.substr(pos + 1);
	int uref = atoi(urefstr.substr(1).c_str());
	pos = rrefstr.find(".zip");
	if (pos == std::string::npos)
		return false;
	rrefstr = rrefstr.substr(0, pos);
	int rref = atoi(rrefstr.substr(1).c_str());

	char* pszSelectMedia = sqlite3_mprintf("SELECT data FROM \"%w\" WHERE uref = ? AND rref = ? ", tablename.c_str());
	sqlite3_stmt *stmt;
	int rc;
	rc = sqlite3_prepare_v2(hDB, pszSelectMedia, -1, &stmt, NULL);
	if (rc == SQLITE_OK)
	{
		sqlite3_bind_int(stmt, 1, uref);
		sqlite3_bind_int(stmt, 2, rref);
		rc = sqlite3_step(stmt);
		if (rc == SQLITE_ROW)
		{
			int size = sqlite3_column_bytes(stmt, 0);
			fin.rdbuf()->pubsetbuf((char *)sqlite3_column_blob(stmt, 0), size);
			rc = sqlite3_finalize(stmt);
			if (rc == SQLITE_SCHEMA)
				return false;
		}
		else 
			return false;
	}
	else
		return false;
	return true;
}
#endif

bool CDB_Global::ParseGSKey(const std::string &mediaId, GSMediaKey &Key, std::string &gpkgtablename, GSTableType &TableType)
{
	if (mediaId.substr(0, 5) != "gpkg:")
		return false;

	TableType = GSInvalid;
	size_t pos;
	if (pos = mediaId.find("300_GSModelGeometry") != std::string::npos)
	{
		TableType = GSGeometry;
	}
	else if(pos = mediaId.find("301_GSModelTexture") != std::string::npos)
	{
			TableType = GSTexture;
	}
	else if ((pos = mediaId.find("GTModelGeometry") != std::string::npos))
	{
		TableType = GTGeometry;
	}
	else if ((pos = mediaId.find("GTModelTexture") != std::string::npos))
	{
		TableType = GTTexture;
	}

	std::string tablename = mediaId.substr(5);
	int lod;
	int uref;
	int rref;
	if ((TableType == GSGeometry) || (TableType == GSTexture))
	{
		pos = tablename.find_first_of(":");
		if (pos == std::string::npos)
			return false;
		std::string refstring = tablename.substr(pos + 1);
		tablename = tablename.substr(0, pos);

		pos = tablename.find_last_of("_L");
		if (pos == std::string::npos)
			return false;
		std::string LodStr = tablename.substr(pos + 1);
		lod = atoi(LodStr.c_str());
		pos = refstring.find_first_of(":");
		if (pos == std::string::npos)
			return false;

		std::string urefstr = refstring.substr(0, pos);
		std::string rrefstr = refstring.substr(pos + 1);
		uref = atoi(urefstr.substr(1).c_str());
		pos = rrefstr.find(".zip");
		if (pos == std::string::npos)
			return false;
		rrefstr = rrefstr.substr(0, pos);
		rref = atoi(rrefstr.substr(1).c_str());
	}
	else
	{
		pos = tablename.find(".zip");
		if (pos == std::string::npos)
			return false;
		tablename = tablename.substr(0, pos);
		lod = 0;
		uref = 0;
		rref = 0;
	}


	Key.LODNum = lod;
	Key.UrefNum = uref;
	Key.RrefNum = rref;
	gpkgtablename = tablename;
	return true;
}



GDALDataset * CDB_Global::Get_Dataset(void)
{
	return (GDALDataset *)m_ogrDataset;
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