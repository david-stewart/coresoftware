#include "CDBNtuple.h"

#include <TFile.h>
#include <TNtuple.h>
#include <TSystem.h>
#include <TTreeReader.h>
#include <TTreeReaderValue.h>

#include <iostream>

CDBNtuple::CDBNtuple(const std::string &fname)
  : m_Filename(fname)
{
}

CDBNtuple::~CDBNtuple()
{
  m_EntryMap.clear();
  m_SingleEntryMap.clear();
}

void CDBNtuple::SetValue(int channel, const std::string &name, float value)
{
  if (name == "ID")
  {
    std::cout << "Sorry ID is reserved as fieldname, pick anything else" << std::endl;
    gSystem->Exit(1);
  }
  if (m_Locked[MultipleEntries])
  {
    std::cout << "Trying to add field " << name << " after another entry was committed" << std::endl;
    std::cout << "That does not work, restructure your code" << std::endl;
    gSystem->Exit(1);
  }
  m_EntryMap[channel].insert(std::make_pair(name,value));
  m_EntryNameSet.insert(name);
}

void CDBNtuple::Commit()
{
  m_Locked[MultipleEntries] = true;
// create ntuple string
  std::map<std::string, unsigned int> index;
  index.insert(std::make_pair("ID",0));
  std::string fieldnames = "ID";
  unsigned int i = 0;
  for (auto &field :  m_EntryNameSet)
  {
    fieldnames += ":";
    fieldnames += field;
    i++;
    index.insert(std::make_pair(field,i));
  }
  float *vals = new float[m_EntryNameSet.size()+1]; // +1 for the id
  std::fill(vals,vals+m_EntryNameSet.size()+1,NAN);
  if (m_TNtuple[MultipleEntries] == nullptr)
  {
    m_TNtuple[MultipleEntries] = new TNtuple(m_TNtupleName[MultipleEntries].c_str(),m_TNtupleName[MultipleEntries].c_str(),fieldnames.c_str());
  }
  for (auto &entry :  m_EntryMap)
  {
    vals[(index.find("ID")->second)] = entry.first;
    for (auto &calib : entry.second)
    {
      vals[(index.find(calib.first)->second)] = calib.second;  
    }
    m_TNtuple[MultipleEntries]->Fill(vals);
    std::fill(vals,vals+m_EntryNameSet.size()+1,NAN);
  }
  delete [] vals;
  return;
}

void CDBNtuple::SetSingleValue(const std::string &name, float value)
{
  if (m_SingleEntryMap.find(name) == m_SingleEntryMap.end())
  {
    if (m_Locked[SingleEntries])
    {
      std::cout << "Trying to add field " << name << " after another entry was committed" << std::endl;
      std::cout << "That does not work, restructure your code" << std::endl;
      gSystem->Exit(1);
    }
    m_SingleEntryMap.insert(std::make_pair(name, value));
    return;
  }
  m_SingleEntryMap[name] = value;
}

void CDBNtuple::CommitSingle()
{
  m_Locked[SingleEntries] = true;
  float *vals = new float[m_SingleEntryMap.size()];
  std::string fieldnames;
  size_t i=0;
  for (auto &field :  m_SingleEntryMap)
  {
    fieldnames += field.first;
    vals[i] =   field.second;  
    i++;
    if (i < m_SingleEntryMap.size())
    {
      fieldnames += ':';
    }
  }
  m_TNtuple[SingleEntries] = new TNtuple(m_TNtupleName[SingleEntries].c_str(),m_TNtupleName[SingleEntries].c_str(),fieldnames.c_str());
  m_TNtuple[SingleEntries]->Fill(vals);
  delete [] vals;
  return;
}

void CDBNtuple::Print()
{
  if (! m_EntryMap.empty())
  {
    std::cout << "Number of entries: " << m_EntryMap.size() << std::endl;
    for (auto &field : m_EntryMap)
    {
      std::cout << "ID: " << field.first << std::endl;
      for (auto &calibs : field.second)
      {
	std::cout << "name " << calibs.first << " value: " << calibs.second << std::endl;
      }
    }
  }
  if (!m_SingleEntryMap.empty())
  {
    if (!m_EntryMap.empty())
    {
      std::cout << std::endl;
    }
    std::cout << "Number of single fields: " << m_SingleEntryMap.size() << std::endl;
    for (auto &field : m_SingleEntryMap)
    {
      std::cout << field.first << " value " << field.second << std::endl;
    }
  }
}

void CDBNtuple::WriteCDBNtuple()
{
  TFile *f = TFile::Open(m_Filename.c_str(),"RECREATE");
  for (auto ntup : m_TNtuple)
  {
    if (ntup != nullptr)
    {
      ntup->Write();
    }
  }
  f->Close();
}

void CDBNtuple::LoadCalibrations()
{
  TFile *f = TFile::Open(m_Filename.c_str());
  f->GetObject(m_TNtupleName[SingleEntries].c_str(), m_TNtuple[SingleEntries]);
  f->GetObject(m_TNtupleName[MultipleEntries].c_str(), m_TNtuple[MultipleEntries]);
  if (m_TNtuple[SingleEntries] != nullptr)
  {
    TObjArray *branches =  m_TNtuple[SingleEntries]->GetListOfBranches();
    TIter iter(branches);
    while(TBranch *thisbranch = static_cast<TBranch *> (iter.Next()))
    {
      float val;
      m_TNtuple[SingleEntries]->SetBranchAddress(thisbranch->GetName(),&val);
      thisbranch->GetEntry(0);
      m_SingleEntryMap.insert(std::make_pair(thisbranch->GetName(), val));
    }
  }
  if (m_TNtuple[MultipleEntries] != nullptr)
  {
    TObjArray *branches =  m_TNtuple[MultipleEntries]->GetListOfBranches();
    TIter iter(branches);
    std::map<std::string, float> branchmap;
    while(TBranch *thisbranch = static_cast<TBranch *> (iter.Next()))
    {
      float val;
      std::pair<std::string, float> valpair = std::make_pair(thisbranch->GetName(),val);
      branchmap.insert(valpair);
    }
    for (auto &biter : branchmap)
    {
      m_TNtuple[MultipleEntries]->SetBranchAddress(biter.first.c_str(), &biter.second);
    }
    for (int i=0; i<m_TNtuple[MultipleEntries]->GetEntries(); i++)
    {
      m_TNtuple[MultipleEntries]->GetEntry(i);
      int id = branchmap.find("ID")->second;
      m_EntryMap.insert(std::make_pair(id, branchmap));
    }
  }
  f->Close();
}

float CDBNtuple::GetSingleValue(const std::string &name)
{  
  if ( m_SingleEntryMap.empty())
  {
    LoadCalibrations();
  }
  auto singleiter = m_SingleEntryMap.find(name);
  if (singleiter == m_SingleEntryMap.end())
  {
    std::cout << "Could not find " << name << " in single calibrations" << std::endl;
    std::cout << "Existing values:" << std::endl;
    for (auto &eiter : m_SingleEntryMap)
    {
      std::cout << "name : " << eiter.first << ", value " << eiter.second
		<< std::endl;
    }
  }
  return singleiter->second ;
}

float CDBNtuple::GetValue(int channel, const std::string &name)
{
  if ( m_EntryMap.empty())
  {
    LoadCalibrations();
  }
  auto channelmapiter = m_EntryMap.find(channel);
  if (channelmapiter == m_EntryMap.end())
  {
    std::cout << "Could not find channel " << channel << " in calibrations" << std::endl;
    return NAN;
  }
  auto calibiter = channelmapiter->second.find(name);
  if (channelmapiter->second.find(name) == channelmapiter->second.end())
  {
    std::cout << "Could not find " << name << " among calibrations" << std::endl;
    return NAN;
  }
  return calibiter->second;
}
