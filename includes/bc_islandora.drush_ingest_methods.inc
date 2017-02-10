<?php

/**
 * @file
 * Ingest methods for `drush bc_islandora_ingest_objects`.
 */

require_once 'bc_islandora.drush_ingest_helpers.inc';

/**
 * Ingests a newspaper object.
 *
 * Sample CSV data:
 * title,date,filename,"page number",type,extent
 * "Barnard Bulletin, October 1, 1990","October 1, 1990",,,issue,24
 */
function _bc_islandora_drush_ingest_newspaper($query_file, $options = array()) {
  module_load_include('inc', 'islandora_paged_content', 'includes/utilities');
  module_load_include('inc', 'islandora_paged_content', 'includes/batch');

  ini_set("auto_detect_line_endings", "1");

  // If the generate-csv option is specified or if the argument supplied is a
  // directory, generate a csv input file first.
  if ($options['generate-csv'] !== NULL || is_dir($query_file)) {
    $csv_file = _bc_islandora_drush_generate_newspaper_csv($query_file, $options);
    drush_log("Generated CSV file, please review it before continuing.", 'error');
    return;
    // $fh = fopen($csv_file, 'r');
  }
  else {
    $fh = fopen($query_file, 'r');
  }

  $path_name = empty($csv_file) ? dirname($query_file) : dirname($csv_file);
  $line_number = 0;
  $issues = array();

  if (!$fh) {
    drush_log("Couldn't open file.", 'error');
    return;
  }
  while (($data = fgetcsv($fh)) !== FALSE) {
    $line_number++;
    // Only parse the data if we aren't on the first line, which contains
    // column headings.
    if ($line_number > 1) {
      $type = $data[4];
      if ($type == 'issue') {
        // Sample issue pid: bc:bulletin-19901001.
        $issue_pid = 'bc:bulletin-' . date('Ymd', strtotime($data[1]));

        // If the issue doesn't exist, create it.
        if (!islandora_object_load($issue_pid)) {
          $issue_pid = create_issue_object($issue_pid, $data);
        }
        // If it exists, give the user the option to overwrite it.
        // (N.B. this process will also allow the user to remove all
        // associated issue pages.)
        else {
          $continue = drush_prompt(dt("Object exists at pid = @pid. Overwrite? (y/n)", array('@pid' => $issue_pid)));
          if (preg_match('/^y/', strtolower($continue)) === 1) {
            drush_bc_islandora_purge_object($issue_pid);
            $issue_pid = create_issue_object($issue_pid, $data);
          }
        }
        $issues[] = $issue_pid;
      }
      elseif ($type == 'page') {
        // Sample issue pid: bc:bulletin-19901001.
        // Sample page pid: bc:bulletin-19901001-1.
        $issue_pid = 'bc:bulletin-' . date('Ymd', strtotime($data[1]));
        $page_pid = 'bc:bulletin-' . date('Ymd', strtotime($data[1])) . '-' . $data[3];

        if (!islandora_object_load($page_pid)) {
          create_page_object($path_name, $data, $issue_pid, $page_pid, 'newspaper');
        }
        // If we found an existing page, prompt user on action.
        else {
          $continue = drush_prompt(dt("Page exists at pid = @pid. Overwrite? (y/n)", array('@pid' => $page_pid)));
          if (preg_match('/^y/', strtolower($continue)) === 1) {
            drush_bc_islandora_purge_object($page_pid);
            create_page_object($path_name, $data, $issue_pid, $page_pid, 'newspaper');
          }
        }
      }
    }
  }
  // TODO should we keep this?
  // _bc_islandora_create_derivatives($issues);
}

/**
 * Ingests a book.
 */
function _bc_islandora_drush_ingest_book($query_file, $options = array()) {
  module_load_include('inc', 'islandora_paged_content', 'includes/utilities');
  module_load_include('inc', 'islandora_paged_content', 'includes/batch');

  ini_set("auto_detect_line_endings", "1");

  $fh = fopen($query_file, 'r');
  $year = isset($options['year']) ? $options['year'] : NULL;
  $books = array();

  while (($data = fgetcsv($fh)) !== FALSE) {
    $csv_year = $data[3];
    if ($year && $year != $csv_year) {
      continue;
    }
    // Some tricky stuff follows to handle books with multiple "volumes."
    $vol_parts = explode(' ', $data[1]);
    array_shift($vol_parts);
    $book_pid = 'bc:yearbook-' . implode('.', $vol_parts);
    $pages = $data[12];

    if (!islandora_object_load($book_pid)) {
      drush_log("Creating book at pid = @pid", array('@pid' => $book_pid), 'ok');
      $book_pid = create_book_object($book_pid, $data);
    }
    else {
      $continue = drush_prompt(dt("Object exists at pid = @pid. Overwrite? (y/n)", array('@pid' => $issue_pid)));
      if (preg_match('/^y/', strtolower($continue)) === 1) {
        drush_log(dt("Removing object: @pid", array('@pid' => $issue_pid)), 'ok');
        drush_bc_islandora_purge_object($issue_pid);
        $book_pid = create_book_object($book_pid, $data);
      }
      else {
        continue;
      }
    }
    $books[] = $book_pid;

    // Create each page.
    $file_path = dirname($query_file) . '/' . $csv_year;
    if (file_exists($file_path)) {
      $files = explode("\n", `ls $file_path/*.jp2`);
      foreach ($files as $fn) {
        if (!empty($fn)) {
          $page_number = preg_replace('/^0+/', '', preg_replace('/\.jp2$/', '', array_pop(explode('_', $fn))));
          $data[] = $page_number;
          $page_pid = $book_pid . '-' . $page_number;

          if (!islandora_object_load($page_pid)) {
            create_page_object($fn, $data, $book_pid, $page_pid, 'book');
          }
          else {
            drush_log(dt("Page exists at pid = @pid. Skipping...", array('@pid' => $page_pid)), 'warning');
          }
        }
      }
    }
    else {
      drush_log(dt("File not found: @file_path", array('@file_path' => $file_path)), 'error');
    }
  }
  // TODO should we keep this?
  _bc_islandora_create_derivatives($books);
}

/**
 * Ingests student publications.
 *
 * @param string $directory
 *   The directory containing publications to ingest.
 */
function _bc_islandora_drush_ingest_student_publication($directory, $options = array()) {
  $pub_id = $options['identifier'];

  if (is_dir($directory)) {
    // Look at each file in the supplied (argument) directory.
    foreach (scandir($directory) as $file) {
      // The full path to this file.
      $path = $directory . '/' . $file;
      // The XML data file (should follow the format: [directory name].xml).
      $xml_file = $path . '/' . $file . '.xml';
      // If we didn't get a good directory or XML file, move on.
      if (!is_dir($path) || preg_match('/^\./', $file) !== 0 || !file_exists($xml_file)) {
        continue;
      }
      $pub_data = _bc_islandora_pub_data($xml_file);
      $pid_title = str_replace(' ', '-', preg_replace('/[^a-z\d\s+]/', '', strtolower($pub_data['title'])));
      $pub_pid = 'bc:' . $pid_title;
      // Publication ids that constitute "exceptions" (different behavior
      // required).
      $exceptions = array('BC12-9_1959', 'BC12-26_1970', 'BC12-30_C19711972');
      $exception = in_array(array_pop(explode('/', $path)), $exceptions);
      // If pub object doesn't exist, create it.
      if (!islandora_object_load($pub_pid) && !$exception) {
        create_pub_object($pub_pid, $pub_data);
      }
      elseif ($exception) {
        $collection_pid = variable_get('bc_islandora_pubs_pid', 'islandora:1022');
        $book_pid = create_book_object($pub_pid, _bc_islandora_pub_data($xml_file, $collection_pid));
      }
      else {
        $continue = drush_prompt(dt("Object exists at pid = @pid. Overwrite? (y/n)", array('@pid' => $pub_pid)));
        if (preg_match('/^y/', strtolower($continue)) === 1) {
          drush_log(dt("Removing object: @pub_pid", array('@pub_pid' => $pub_pid)), 'ok');
          drush_bc_islandora_purge_object($pub_pid);
          create_pub_object($pub_pid, $pub_data);
        }
      }
      $pub_obj = islandora_object_load($pub_pid);
      // If we got an id, only look at the corresponding dir.
      if (($pub_id && $file == $pub_id) || !$pub_id) {
        if ($exception) {
          // Yuk!
          if (!isset($book_pid)) {
            $book_pid = $pub_pid;
          }
          _bc_islandora_process_pub_pages($book_pid, $xml_file, $path, $purge);
        }
        // If non-exceptional, do the normal thing (TODO reverse this order?)
        else {
          // Create a book object corresponding to each pub.
          foreach (scandir($path) as $subfile) {
            $subpath = implode('/', array($path, $subfile));
            $xml_file = implode('/', array($subpath, $subfile)) . '.xml';
            if ((!is_dir($subpath) || preg_match('/^\./', $subdir) !== 0 || !file_exists($xml_file)) && !$exception) {
              continue;
            }
            $book_data = _bc_islandora_pub_data($xml_file, $pub_pid, 'book');
            $issue_pid = implode('-', array($pub_pid, $book_data['date']));
            if (!islandora_object_load($issue_pid)) {
              create_book_object($issue_pid, $book_data);
            }
            else {
              $continue = drush_prompt(dt("Object exists at pid = @pid. Overwrite? (y/n)", array('@pid' => $issue_pid)));
              if (preg_match('/^y/', strtolower($continue)) === 1) {
                drush_log(dt("Removing object: @pub_pid", array('@pub_pid' => $issue_pid)), 'ok');
                drush_bc_islandora_purge_object($issue_pid);
                create_book_object($issue_pid, $book_data);
              }
            }
            $issue_obj = islandora_object_load($issue_pid);
            // Create a page object for each page.
            _bc_islandora_process_pub_pages($issue_pid, $xml_file, $subpath);
          }
        }
      }
    }
  }
}

/**
 * Ingests manuscripts.
 */
function _bc_islandora_drush_ingest_manuscript($md_file, $options) {
  $id_option = $options['identifier'];
  if (file_exists($md_file)) {
    $md_fh = fopen($md_file, 'r');
    while (($data = fgetcsv($md_fh)) !== FALSE) {
      $ms_data = array();
      $ms_data['id'] = $data[0];
      // Skip this row if we got an id option and this id doesn't match.
      if ($id_option && $id_option != $ms_data['id']) {
        drush_log("Skipped due to ID mismatch: " . $ms_data['id'], 'error');
        continue;
      }
      $ms_data_dir = dirname($md_file) . '/' . $ms_data['id'];
      if (is_dir($ms_data_dir)) {
        $ms_data['title'] = $data[1];
        $ms_mods_file = $ms_data_dir . '/' . $ms_data['id'] . '.xml';
        // If we found an XML file that matches the naming convention, assume
        // it contains the MODS data we want.
        if (file_exists($ms_mods_file)) {
          drush_log("Found XML mods file for: " . $ms_data['id'], 'ok');
          $ms_data['xml'] = file_get_contents($ms_mods_file);
        }
        // Otherwise, generate MODS using the other csv fields.
        else {
          drush_log("Building mods from CSV MD file for: " . $ms_data['id'], 'ok');

          $ms_data['creator'] = $data[2];
          $ms_data['creator_uri'] = $data[3];
          $ms_data['creator1'] = $data[4];
          $ms_data['creator1_uri'] = $data[5];
          $ms_data['creator2'] = $data[6];
          $ms_data['creator2_uri'] = $data[7];
          $ms_data['creator3'] = $data[8];
          $ms_data['creator3_uri'] = $data[9];
          $ms_data['resource_type'] = $data[10];
          $ms_data['genre'] = $data[11];
          $ms_data['genre_uri'] = $data[12];
          $ms_data['date'] = $data[13];
          // $ms_data['date_qualifier'] = $data[14];
          // NB start and end date only present if date is approx range...
          $ms_data['date_start'] = $data[15];
          $ms_data['date_end'] = $data[16];
          $ms_data['extent'] = $data[17];
          $ms_data['description'] = $data[18];
          $ms_data['location'] = $data[19];
          $ms_data['topic'] = $data[20];
          $ms_data['topic_uri'] = $data[21];
        }

        $ms_pid = 'bcrw';

        // Create a new pub for all files, using next available INT in namespace.
        if (!islandora_object_load($ms_pid)) {
          drush_log("Creating publication object for: " . $ms_data['id'], 'ok');
          $ms_data['collection_pid'] = variable_get('bc_islandora_manuscripts_pid', 'islandora:manuscriptCollection');
          $ms_data['cm_pid'] = 'islandora:manuscriptCModel';
          $ms_data['xml'] = _bc_islandora_ms_mods($ms_data);
          $ms_parent_pid = create_pub_object($ms_pid, $ms_data);
        }
        else {
          drush_log(dt('Object exists: @ms_pid', array('@ms_pid' => $ms_pid)), 'ok');
        }

        foreach (scandir($ms_data_dir) as $ms_file) {
          // Create a page object for each tif file we find.
          if (preg_match('/\.tif$/', $ms_file)) {
            drush_log("Creating page object for: " . $ms_file, 'ok');
            $page_padded = str_replace('.tif', '', array_pop(explode('_', $ms_file)));
            $page_number = preg_replace('/^0+/', '', $page_padded);
            $page_pid = $ms_pid . '-' . $page_number;
            $filepath = $ms_data_dir . '/' . $ms_file;
            $ms_data['id'] = $data[0] . '_' . $page_padded;
            $ms_data['page_number'] = $page_number;
            $ms_data['title'] = $data[1] . ', page ' . $page_number;
            $ms_data['cm_pid'] = $ms_pid;
            $ms_data['type'] = 'page';
            $ms_data['xml'] = _bc_islandora_ms_mods($ms_data);

            // Check if a text file is present as OCR.
            $ocr_file = $ms_data_dir . '/' . str_replace('.tif','.txt',$ms_file);
            $ms_data['ocr'] = file_exists($ocr_file) ? $ocr_file : "";

            // TODO this is redundant now. Find another way to check if page
            // exists?
            if (!islandora_object_load($page_pid)) {
              create_page_object($ms_data_dir . '/' . $ms_file, $ms_data, $ms_parent_pid, 'bcrw', 'ms');
            }
            else {
              drush_log(dt('Object exists: @page_pid', array('@page_pid' => $page_pid)), 'ok');
            }
          }
        }
      }
    }
  }
  else {
    drush_log(dt("File not found: @md_file", array('@md_file' => $md_file)), 'error');
  }
}

/**
 * Ingests diaries.
 *
 * This ingest system is annoying but slowly becoming more robust after manus,
 * and now diaries.
 *
 * Diaries use Islandora Book Model and Page Models.
 *
 * Barnard PID Examples:
 * BC15-12 (Diary of Stella Bloch...)
 * * diary collection: BC15-12:0 (The collection will be at 0.)
 * * book level - BC15-12:1
 * * page level - BC15-12:2,...,n
 *
 * Let's look at our folder structure, and what this script expects.
 * ./diaries_metadata.csv
 * ./BC15-12_diaryxyz/pages[0-999].tiff
 * ./BC15-12_diaryxyz/pages[0-999].txt (*optional OCR stream to ingest)
 *
 * @file diaries_metadata.csv see: https://docs.google.com/spreadsheets/d/1VGg6RIV9httTIL5F-Vy_0EpWXfne4xTiG2mFZmO5dwA/edit#gid=195535030
 */
function _bc_islandora_drush_ingest_diaries($md_file, $options) {
  // Return early if md_file is not found.
  if (!file_exists($md_file)) {
    drush_log(dt("File not found: @md_file", array('@md_file' => $md_file)), 'error');
    return;
  }

  $id_option = $options['identifier'];

  $md_fh = fopen($md_file, 'r');
  while (($data = fgetcsv($md_fh)) !== FALSE) {
    $csv_row = array();
    $csv_row['id'] = $data[0];
    $filename_array = explode('_', $data[0]);
    $ns = $filename_array[0];
    $id = $filename_array[1];
    // Skip this row if we got an id option and this id doesn't match.
    if ($id_option && $id_option != $ns) {
      drush_log("Skipped due to ID mismatch: " . $csv_row['id'], 'error');
      continue;
    }
    $csv_directory = dirname($md_file) . '/' . $id;
    if (is_dir($csv_directory)) {
      $csv_row['title'] = $data[1];
      $ms_mods_file = $csv_directory . '/' . $csv_row['id'] . '.xml';
      // If we found an XML file that matches the naming convention, assume
      // it contains the MODS data we want.
      if (file_exists($ms_mods_file)) {
        drush_log("Found XML mods file for: " . $csv_row['id'], 'ok');
        $csv_row['xml'] = file_get_contents($ms_mods_file);
      }
      // Otherwise, generate MODS using the other csv fields.
      else {
        drush_log("Building mods from CSV MD file for: " . $csv_row['id'], 'ok');
        $csv_row['id'] = $data[0];
        $csv_row['title'] = $data[1];
        // This can be automated by including a header row, and figuring out
        // how many 'topics' there are, their location, etc. The mods script
        // that fires after this CAN HANDLE multiples.
        $csv_row['creators'][0]['name'] = $data[2];
        $csv_row['creators'][0]['uri'] = $data[3];
        $csv_row['resource_type'] = $data[4];
        $csv_row['genre'] = $data[5];
        $csv_row['genre_uri'] = $data[6];
        $csv_row['date_exact'] = $data[7];
        $csv_row['date_start_approx'] = $data[8];
        $csv_row['date_end_approx'] = $data[9];
        // NB start and end date only present if date is approx range...
        $csv_row['date_start_exact'] = $data[10];
        $csv_row['date_end_exact'] = $data[11];
        $csv_row['extent'] = $data[12];
        $csv_row['description'] = $data[13];
        $csv_row['location'] = $data[14];
        $csv_row['related_item_title'] = $data[15];
        $csv_row['related_item_pid'] = $data[16];
        // This can be automated by including a header row, and figuring out
        // how many 'topics' there are, their location, etc. The mods script
        // that fires after this CAN HANDLE multiples.
        $csv_row['topics'][0]['topic'] = $data[17];
        $csv_row['topics'][0]['uri'] = $data[18];
        $csv_row['topics'][1]['topic'] = $data[19];
        $csv_row['topics'][1]['uri'] = $data[20];
        $csv_row['topics'][2]['topic'] = $data[21];
        $csv_row['topics'][2]['uri'] = $data[22];
        $csv_row['topics'][3]['topic'] = $data[23];
        $csv_row['topics'][3]['uri'] = $data[24];
        $csv_row['topics'][4]['topic'] = $data[25];
        $csv_row['topics'][4]['uri'] = $data[26];
        $csv_row['topics'][5]['topic'] = $data[27];
        $csv_row['topics'][5]['uri'] = $data[28];
        $csv_row['topics'][6]['topic'] = $data[29];
        $csv_row['topics'][6]['uri'] = $data[30];
      }

      // Now we can create our object.
      $book_object_pid = $ns;
      // Create a new pub for all files, using next available INT in namespace.
      if (!islandora_object_load($book_object_pid)) {
        drush_log("Creating diary (book) object for: " . $csv_row['id'], 'ok');
        $csv_row['collection_pid'] = variable_get('bc_islandora_diaries_pid', 'islandora:bookCollection');
        $csv_row['cm_pid'] = 'islandora:bookCModel';
        $csv_row['xml'] = _bc_islandora_diaries_mods($csv_row, 'book');
        drush_log($csv_row['xml'], 'ok');
        // $ms_parent_pid = create_pub_object($book_object_pid, $csv_row);
      }
      else {
        drush_log(dt('Object exists: @pid', array('@pid' => $book_object_pid)), 'ok');
      }

      foreach (scandir($csv_directory) as $page_proc) {
        // Create a page object for each tif file we find.
        if (preg_match('/\.tif$/', $page_proc)) {
          drush_log("Creating page object for: " . $page_proc, 'ok');
          $page_padded = str_replace('.tif', '', array_pop(explode('_', $page_proc)));
          $page_number = preg_replace('/^0+/', '', $page_padded);
          $page_pid = $book_object_pid . '-' . $page_number;
          $filepath = $csv_directory . '/' . $page_proc;
          $csv_row['id'] = $data[0] . '_' . $page_padded;
          $csv_row['page_number'] = $page_number;
          $csv_row['title'] = $data[1] . ', page ' . $page_number;
          $csv_row['cm_pid'] = 'islandora:pageCModel';
          $csv_row['type'] = 'page';
          $csv_row['xml'] = _bc_islandora_diaries_mods($csv_row, 'page');

          // Check if a text file is present as OCR.
          $ocr_file = $csv_directory . '/' . str_replace('.tif', '.txt', $page_proc);
          $csv_row['ocr'] = file_exists($ocr_file) ? $ocr_file : "";

          // TODO this is redundant now. Find another way to check if page
          // exists?
          if (!islandora_object_load($page_pid)) {
            drush_log($csv_row['xml'], 'ok');
            // create_page_object($csv_directory . '/' . $page_proc, $csv_row, $ms_parent_pid, 'bcrw', 'ms');
          }
          else {
            drush_log(dt('Object exists: @page_pid', array('@page_pid' => $page_pid)), 'ok');
          }
        }
      }
    }
  }
}
