<?php

function _barnard_dc_drush_np_issue_mods($title, $extent, $date, $date_short) {
  return <<<ISSUE_MODS
<mods xmlns="http://www.loc.gov/mods/v3" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.loc.gov/mods/v3 http://www.loc.gov/mods/v3/mods-3-5.xsd">
  <titleInfo>
    <title>$title</title>
  </titleInfo>
  <genre authority="marcgt">newspaper</genre>
  <physicalDescription>
    <form authority="marcform">print</form>
    <form authority="marcform">electronic</form>
    <extent>$extent pages</extent>
    <internetMediaType>image/tiff</internetMediaType>
    <digitalOrigin>reformatted digital</digitalOrigin>
  </physicalDescription>
  <relatedItem type="host"> 
    <titleInfo> 
      <title>Barnard Bulletin</title> 
    </titleInfo> 
    <typeOfResource>text</typeOfResource> 
    <identifier type="local">BC12-03</identifier> 
  </relatedItem> 
  <originInfo>
    <dateIssued encoding="w3cdtf">$date</dateIssued>
    <dateCreated encoding="w3cdtf">$date</dateCreated>
  </originInfo>
  <identifier type="local">BC12-03_$date_short</identifier>
</mods>
ISSUE_MODS;
}

function _barnard_dc_drush_page_mods($type, $title, $genre, $host_title, $host_id, $page_id, $date_created, $date_issued) {
  switch ($type) {
    case 'pub':
      $mods_xml = <<<PUB_PAGE_MODS
<mods xmlns="http://www.loc.gov/mods/v3" xmlns:mods="http://www.loc.gov/mods/v3" xmlns:xlink="http://www.w3.org/1999/xlink" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
  <titleInfo>
    <title>$title</title>
  </titleInfo>
  <genre authority="lcgft" authorityURI="http://id.loc.gov/authorities/genreForms" valueURI="http://id.loc.gov/authorities/genreForms/gf2014026139">$genre</genre>
  <relatedItem type="host" displayLabel="Record Group">
    <titleInfo>
      <title>Student Publications</title>
    </titleInfo>
  </relatedItem>
  <relatedItem type="host">
    <titleInfo>
      <title>$host_title</title>
    </titleInfo>
    <typeOfResource>text</typeOfResource>
    <identifier type="local">$host_id</identifier>
  </relatedItem>
  <identifier type="local">$page_id</identifier>
  <originInfo>
    <dateCreated encoding="w3cdtf">$date_created</dateCreated>
  </originInfo>
</mods>
PUB_PAGE_MODS;
    break;
    case 'newspaper':
    case 'book':
    default:
      $mods_xml = <<<DEFAULT_PAGE_MODS
<mods xmlns="http://www.loc.gov/mods/v3" xmlns:mods="http://www.loc.gov/mods/v3" xmlns:xlink="http://www.w3.org/1999/xlink" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
  <titleInfo>
    <title>$host_title</title>
  </titleInfo>
  <genre authority="marcgt">$genre</genre>
  <originInfo>
    $date_issued
    $date_created
  </originInfo>
  <relatedItem type="host"> 
    <titleInfo> 
      <title>$title</title> 
    </titleInfo> 
    <typeOfResource>text</typeOfResource> 
    <identifier type="local">$host_id</identifier> 
  </relatedItem> 
  <identifier type="local">$page_id</identifier>
</mods>
DEFAULT_PAGE_MODS;
    break;
  }
  return $mods_xml;
}

function _barnard_dc_drush_book_mods($title, $date, $extent, $description, $id_date) {
	return <<<BOOK_MODS
<mods xmlns="http://www.loc.gov/mods/v3" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.loc.gov/mods/v3 http://www.loc.gov/mods/v3/mods-3-5.xsd">
  <titleInfo>
    <title>$title</title>
  </titleInfo>

  <name type="corporate" authority="naf" authorityURI="http://id.loc.gov/authorities/names" valueURI="http://id.loc.gov/authorities/names/n79065105">
    <namePart>Barnard College</namePart>
    <role>
      <roleTerm type="text" authority="marcrelator">Publisher</roleTerm>
    </role>
  </name>

  <typeOfResource>text</typeOfResource>

  <genre authority="marcgt">yearbook</genre>

  <originInfo eventType="publication">
    <place>
      <placeTerm type="code" authority="marccountry">nyu</placeTerm>
      <placeTerm type="text">New York (N.Y.)</placeTerm>
    </place>
    <publisher>Barnard College</publisher>
    <dateIssued encoding="iso8601" keyDate="yes">$date</dateIssued>
    <dateCreated encoding="iso8601" keyDate="yes">$date</dateCreated>
    <issuance>continuing</issuance>
  </originInfo>

  <language>
    <languageTerm type="code" authority="iso639-2b">eng</languageTerm>
  </language>

  <physicalDescription>
    <form authority="marcform">print</form>
    <form authority="marcform">electronic</form>
    <extent>$extent</extent>
    <internetMediaType>image/jpeg2000</internetMediaType>
    <digitalOrigin>reformatted digital</digitalOrigin>
  </physicalDescription>

  <abstract>$description</abstract>

  <subject authority="lcsh" authorityURI="http://id.loc.gov/authorities/subjects" valueURI="http://id.loc.gov/authorities/subjects/sh85147738">
    <topic>Women's colleges</topic>
  </subject>
  <subject authority="lcsh" authorityURI="http://id.loc.gov/authorities/subjects" valueURI="http://id.loc.gov/authorities/subjects/sh85147501">
    <topic>Women college students</topic>
  </subject>

  <subject authority="lcsh" authorityURI="http://id.loc.gov/authorities/subjects" valueURI="http://id.loc.gov/authorities/subjects/sh85129244">
    <topic>Student activities</topic>
  </subject>

  <subject>
    <name type="corporate" authority="naf" authorityURI="http://id.loc.gov/authorities/names" valueURI="http://id.loc.gov/authorities/names/n79065105">
    </name>
  </subject>

  <relatedItem type="host"> 
    <titleInfo> 
      <title>Mortarboard</title> 
    </titleInfo> 
    <typeOfResource>text</typeOfResource> 
    <identifier type="local">BC12-01</identifier> 
  </relatedItem> 

  <identifier type="local">BC12-01_$id_date</identifier>

  <location> 
    <physicalLocation authority="marcorg">NNC</physicalLocation> 
    <physicalLocation authority="marcorg">NyNyBAW</physicalLocation> 
    <physicalLocation>Barnard College Library (New York, N.Y.)</physicalLocation> 
  </location> 

  <accessCondition type="useAndReproduction">This work is licensed under a Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License. Contact the Archives at archives@barnard.edu for more information on access and use, including commercial uses.</accessCondition>

  <recordInfo>
    <recordContentSource>Barnard Archives and Special Collections</recordContentSource>
    <recordContentSource authority="marcorg">NNC</recordContentSource>
    <recordContentSource authority="marcorg">NyNyBAW</recordContentSource>
    <languageOfCataloging>
      <languageTerm authority="iso639-2b">eng</languageTerm>
    </languageOfCataloging>
  </recordInfo>
</mods>
BOOK_MODS;
}

/**
 * Gnerates manuscript MODS.
 */
function _barnard_dc_ms_mods($ms_data) {
  if (isset($ms_data['type']) && $ms_data['type'] == 'page') {
    $date_xml = '<originInfo>';
  }
  else {
    $date_xml = "<originInfo>
    <place>
      <placeTerm type=\"text\" authorityURI=\"http://id.loc.gov/authorities/names\" valueURI=\"http://id.loc.gov/authorities/names/n79007751\">New York (N.Y.)</placeTerm>
    </place>
    <place>
      <placeTerm type=\"code\" authority=\"marccountry\">nyu</placeTerm>
    </place>
    <publisher>Online collection published by Barnard Archives and Special Collections, Barnard College.</publisher>";
    $extent_xml = "
  <physicalDescription>
    <form authority=\"marcform\">print</form>
    <form authority=\"marcform\">elecronic</form>
    <extent>{$ms_data['extent']}</extent>
    <digitalOrigin>reformatted digital</digitalOrigin>
    <internetMediaType>image/tiff</internetMediaType>
  </physicalDescription>";
    $description_xml = "
  <abstract>{$ms_data['description']}</abstract>";
    $location_xml = "
  <location displayLabel=\"not a PURL\">
    <physicalLocation authority=\"marcorg\">NNC</physicalLocation>
    <physicalLocation authority=\"marcorg\">NyNyBAW</physicalLocation>
    <physicalLocation>Barnard College Library (New York, N.Y.)</physicalLocation>
    <shelfLocator>{$ms_data['location']}</shelfLocator>
  </location>";
    $language_xml = "
  <language>
    <languageTerm type=\"text\">English</languageTerm>
    <languageTerm type=\"code\" authority=\"iso639-2b\">eng</languageTerm>
  </language>";
    $access_condition_xml = "
  <accessCondition type=\"useAndReproduction\">Contact the Archives at archives@barnard.edu for more information on access and use.</accessCondition>";
    $record_info_xml = "
  <recordInfo>
    <recordContentSource>Barnard Archives and Special Collections</recordContentSource>
    <recordContentSource authority=\"marcorg\">NNC</recordContentSource>
    <recordContentSource authority=\"marcorg\">NyNyBAW</recordContentSource>
    <languageOfCataloging>
      <languageTerm type=\"text\" authority=\"iso639-2b\">English</languageTerm>
      <languageTerm type=\"code\" authority=\"iso639-2b\">eng</languageTerm>
    </languageOfCataloging>
  </recordInfo>";
  }
  // TODO how to handle multiple creators?
  $creator_xml = "<name type=\"personal\" authority=\"\" authorityURI=\"\" valueURI=\"\">
    <namePart>{$ms_data['creator']}</namePart>
    <role>
      <roleTerm type=\"text\" authority=\"marcrelator\">Creator</roleTerm>
    </role>
  </name>";
  $creator_uri_xml = isset($ms_data['creator_uri']) && !empty($ms_data['creator_uri']) ?
    "<name type=\"personal\" authority=\"naf\" authorityURI=\"http://id.loc.gov/authorities/names\" valueURI=\"{$ms_data['creator_uri']}\">
    <namePart></namePart>
    <role>
      <roleTerm type=\"text\" authority=\"marcrelator\">{$ms_data['creator']}</roleTerm>
    </role>
  </name>" :
    "<name type=\"personal\" authority=\"local\">
    <namePart></namePart>
    <role>
      <roleTerm type=\"text\" authority=\"marcrelator\">{$ms_data['creator']}</roleTerm>
    </role>
  </name>";

  // TODO this should be different depending whether a range is present.
  if (!empty($ms_data['date_start']) && !empty($ms_data['date_end'])) {
    $date_xml .= "
    <dateCreated encoding=\"w3cdtf\" keyDate=\"yes\" point=\"start\" qualifier=\"approximate\">{$ms_data['date_start']}</dateCreated>
    <dateCreated encoding=\"w3cdtf\" point=\"end\" qualifier=\"approximate\">{$ms_data['date_end']}</dateCreated>";
  }
  else {
    $date_xml .= "
    <dateCreated encoding=\"w3cdtf\" keyDate=\"yes\">{$ms_data['date']}</dateCreated>";  
  }
  $date_xml .= "
  </originInfo>";
  $mods_xml = "<mods xmlns=\"http://www.loc.gov/mods/v3\" xmlns:mods=\"http://www.loc.gov/mods/v3\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" xsi:schemaLocation=\"http://www.loc.gov/mods/v3 http://www.loc.gov/mods/v3/mods-3-5.xsd\">
  <identifier type=\"local\">{$ms_data['id']}</identifier>
  <titleInfo>
    <title>{$ms_data['title']}</title>
  </titleInfo>
  {$creator_xml}
  {$creator_uri_xml}
  <typeOfResource>{$ms_data['resource_type']}</typeOfResource>
  <genre authority=\"aat\" authorityURI=\"http://vocab.getty.edu/aat\" valueURI=\"{$ms_data['genre_uri']}\">{$ms_data['genre']}</genre>
  {$date_xml}";
  if (isset($extent_xml)) {
    $mods_xml .= "$extent_xml";
  }
  if (isset($description_xml)) {
    $mods_xml .= "{$description_xml}";
  }
  if (isset($location_xml)) {
    $mods_xml .= "{$location_xml}";
  }
  $mods_xml .= "
  <subject authority=\"lcsh\" authorityURI=\"http://id.loc.gov/authorities/subjects\" valueURI=\"{$ms_data['topic_uri']}\">
    <topic>{$ms_data['topic']}</topic>
  </subject>";
  if (isset($language_xml)) {
    $mods_xml .= "{$language_xml}";
  }
  $mods_xml .= "
  <subject>
    <name type=\"corporate\" authority=\"naf\" authorityURI=\"http://id.loc.gov/authorities/names\" valueURI=\"http://id.loc.gov/authorities/names/n79065105\">
      <namePart>Barnard College</namePart>
    </name>
  </subject>
  <subject>
    <name type=\"corporate\" authority=\"naf\" authorityURI=\"http://id.loc.gov/authorities/names\" valueURI=\"http://id.loc.gov/authorities/names/no90023871\">
      <namePart>Barnard Center for Research on Women</namePart>
    </name>
  </subject>
  <subject authority=\"lcsh\" authorityURI=\"http://id.loc.gov/authorities/subjects\" valueURI=\"http://id.loc.gov/authorities/subjects/sh2008001560\">
    <topic>Second-wave feminism</topic>
  </subject>
  <relatedItem type=\"host\" displayLabel=\"Record Group\">
    <titleInfo>
      <title>Academic Departments and Programs</title>
    </titleInfo>
  </relatedItem>
  <relatedItem type=\"host\">
    <titleInfo>
      <title>Barnard Center for Research on Women</title>
    </titleInfo>
    <identifier type=\"local\">BC13-58</identifier>
    <typeOfResource>mixed material</typeOfResource>
  </relatedItem>";
  if (isset($access_condition_xml)) {
    $mods_xml .= "{$access_condition_xml}";
  }
  if (isset($record_info_xml)) {
    $mods_xml .= "{$record_info_xml}";
  }
  $mods_xml .= "\n</mods>";
  return $mods_xml;
}
