CREATE UNIQUE INDEX cpp_doc_file_descriptor_package_id_parent_null_uniq ON cpp_doc_file_descriptor USING btree (package_id) WHERE parent_id IS NULL;
CREATE UNIQUE INDEX cpp_doc_decl_package_id_parent_null_uniq ON cpp_doc_decl USING btree (package_id) WHERE parent_id IS NULL;

CREATE OR REPLACE FUNCTION get_presumed_loc(p_file_id integer,
                                            p_line integer,
                                            p_col integer) RETURNS integer AS $$
DECLARE
  myrec cpp_doc_presumed_loc%ROWTYPE;
BEGIN
  SELECT id INTO STRICT myrec FROM cpp_doc_presumed_loc WHERE file_id = p_file_id AND line = p_line AND col = p_col;
  RETURN myrec.id;
EXCEPTION
  WHEN NO_DATA_FOUND THEN
    INSERT INTO cpp_doc_presumed_loc (file_id, line, col) VALUES (p_file_id, p_line, p_col) ON CONFLICT DO NOTHING;
    SELECT id INTO STRICT myrec FROM cpp_doc_presumed_loc WHERE file_id = p_file_id AND line = p_line AND col = p_col;
    RETURN myrec.id;
  WHEN TOO_MANY_ROWS THEN
    RAISE EXCEPTION 'presumed_loc not unique';
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION get_file_descriptor(p_package_id integer,
                                               p_parent_id integer,
                                               p_name character varying (4096),
                                               p_path character varying(4096)) RETURNS integer AS $$
DECLARE
  myrec cpp_doc_file_descriptor%ROWTYPE;
BEGIN
  SELECT id INTO STRICT myrec FROM cpp_doc_file_descriptor WHERE package_id = p_package_id AND parent_id = p_parent_id AND name = p_name;
  RETURN myrec.id;
EXCEPTION
  WHEN NO_DATA_FOUND THEN
    INSERT INTO cpp_doc_file_descriptor (package_id, parent_id, name, path) VALUES (p_package_id, p_parent_id, p_name, p_path) ON CONFLICT DO NOTHING;
    SELECT id INTO STRICT myrec FROM cpp_doc_file_descriptor WHERE package_id = p_package_id AND parent_id = p_parent_id AND name = p_name;
    RETURN myrec.id;
  WHEN TOO_MANY_ROWS THEN
    RAISE EXCEPTION 'file_descriptor not unique';
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION get_root_file_descriptor(p_package_id integer) RETURNS integer AS $$
DECLARE
  myrec cpp_doc_file_descriptor%ROWTYPE;
BEGIN
  SELECT id INTO STRICT myrec FROM cpp_doc_file_descriptor WHERE package_id = p_package_id AND parent_id IS NULL;
  RETURN myrec.id;
EXCEPTION
  WHEN NO_DATA_FOUND THEN
    INSERT INTO cpp_doc_file_descriptor (package_id, parent_id, name, path) VALUES (p_package_id, NULL, '', '') ON CONFLICT DO NOTHING;
    SELECT id INTO STRICT myrec FROM cpp_doc_file_descriptor WHERE package_id = p_package_id AND parent_id IS NULL;
    RETURN myrec.id;
  WHEN TOO_MANY_ROWS THEN
    RAISE EXCEPTION 'file_descriptor not unique';
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION get_root_decl(p_package_id integer) RETURNS integer AS $$
DECLARE
  myrec cpp_doc_decl%ROWTYPE;
BEGIN
  SELECT id INTO STRICT myrec FROM cpp_doc_decl WHERE package_id = p_package_id AND parent_id IS NULL;
  RETURN myrec.id;
EXCEPTION
  WHEN NO_DATA_FOUND THEN
    INSERT INTO cpp_doc_decl (package_id, parent_id, name, path, presumed_loc_id) VALUES (p_package_id, NULL, '', '', NULL) ON CONFLICT DO NOTHING;
    SELECT id INTO STRICT myrec FROM cpp_doc_decl WHERE package_id = p_package_id AND parent_id IS NULL;
    RETURN myrec.id;
  WHEN TOO_MANY_ROWS THEN
    RAISE EXCEPTION 'decl not unique';
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION get_decl(p_package_id integer,
                                    p_parent_id integer,
                                    p_name character varying (4096),
                                    p_path character varying(4096)) RETURNS integer AS $$
DECLARE
  myrec cpp_doc_decl%ROWTYPE;
BEGIN
  SELECT id INTO STRICT myrec FROM cpp_doc_decl WHERE package_id = p_package_id AND path = p_path;
  RETURN myrec.id;
EXCEPTION
  WHEN NO_DATA_FOUND THEN
    INSERT INTO cpp_doc_decl (package_id, parent_id, name, path, presumed_loc_id) VALUES (p_package_id, p_parent_id, p_name, p_path, NULL) ON CONFLICT DO NOTHING;
    SELECT id INTO STRICT myrec FROM cpp_doc_decl WHERE package_id = p_package_id AND path = p_path;
    RETURN myrec.id;
  WHEN TOO_MANY_ROWS THEN
    RAISE EXCEPTION 'decl not unique';
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION get_decl(p_package_id integer,
                                           p_parent_id integer,
                                           p_name character varying (4096),
                                           p_path character varying(4096),
                                           p_presumed_loc_id integer) RETURNS integer AS $$
DECLARE
  myrec cpp_doc_decl%ROWTYPE;
BEGIN
  SELECT id INTO STRICT myrec FROM cpp_doc_decl WHERE package_id = p_package_id AND path = p_path;
  UPDATE cpp_doc_decl SET presumed_loc_id = p_presumed_loc_id WHERE id = myrec.id;
  RETURN myrec.id;
EXCEPTION
  WHEN NO_DATA_FOUND THEN
    INSERT INTO cpp_doc_decl (package_id, parent_id, name, path, presumed_loc_id) VALUES (p_package_id, p_parent_id, p_name, p_path, p_presumed_loc_id) ON CONFLICT DO NOTHING;
    SELECT id INTO STRICT myrec FROM cpp_doc_decl WHERE package_id = p_package_id AND path = p_path;
    RETURN myrec.id;
  WHEN TOO_MANY_ROWS THEN
    RAISE EXCEPTION 'decl not unique';
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION get_method_decl(p_decl_id integer,
                                           p_mangled_name character varying (4096),
                                           p_is_const boolean,
                                           p_is_pure boolean,
                                           p_access integer) RETURNS void AS $$
DECLARE
  myrec cpp_doc_method_decl%ROWTYPE;
BEGIN
  SELECT decl_id INTO STRICT myrec FROM cpp_doc_method_decl WHERE decl_id = p_decl_id;
EXCEPTION
  WHEN NO_DATA_FOUND THEN
    INSERT INTO cpp_doc_method_decl (decl_id, mangled_name, is_const, is_pure, access) VALUES (p_decl_id, p_mangled_name, p_is_const, p_is_pure, p_access) ON CONFLICT DO NOTHING;
  WHEN TOO_MANY_ROWS THEN
    RAISE EXCEPTION 'method_decl not unique';
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION get_field_decl(p_decl_id integer,
                                          p_is_mutable boolean,
                                          p_access integer) RETURNS void AS $$
DECLARE
  myrec cpp_doc_method_decl%ROWTYPE;
BEGIN
  SELECT decl_id INTO STRICT myrec FROM cpp_doc_field_decl WHERE decl_id = p_decl_id;
EXCEPTION
  WHEN NO_DATA_FOUND THEN
    INSERT INTO cpp_doc_field_decl (decl_id, is_mutable, access) VALUES (p_decl_id, p_is_mutable, p_access) ON CONFLICT DO NOTHING;
  WHEN TOO_MANY_ROWS THEN
    RAISE EXCEPTION 'field_decl not unique';
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION get_function_decl(p_decl_id integer) RETURNS void AS $$
DECLARE
  myrec cpp_doc_function_decl%ROWTYPE;
BEGIN
  SELECT decl_id INTO STRICT myrec FROM cpp_doc_function_decl WHERE decl_id = p_decl_id;
EXCEPTION
  WHEN NO_DATA_FOUND THEN
    INSERT INTO cpp_doc_function_decl (decl_id) VALUES (p_decl_id) ON CONFLICT DO NOTHING;
  WHEN TOO_MANY_ROWS THEN
    RAISE EXCEPTION 'function_decl not unique';
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION get_namespace_decl(p_decl_id integer) RETURNS void AS $$
DECLARE
  myrec cpp_doc_namespace_decl%ROWTYPE;
BEGIN
  SELECT decl_id INTO STRICT myrec FROM cpp_doc_namespace_decl WHERE decl_id = p_decl_id;
EXCEPTION
  WHEN NO_DATA_FOUND THEN
    INSERT INTO cpp_doc_namespace_decl (decl_id) VALUES (p_decl_id) ON CONFLICT DO NOTHING;
  WHEN TOO_MANY_ROWS THEN
    RAISE EXCEPTION 'namespace_decl not unique';
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION get_record_decl(p_decl_id integer,
                                           p_is_abstract boolean,
                                           p_is_dependent boolean) RETURNS void AS $$
DECLARE
  myrec cpp_doc_record_decl%ROWTYPE;
BEGIN
  SELECT decl_id INTO STRICT myrec FROM cpp_doc_record_decl WHERE decl_id = p_decl_id;
EXCEPTION
  WHEN NO_DATA_FOUND THEN
    INSERT INTO cpp_doc_record_decl (decl_id, is_abstract, is_dependent) VALUES (p_decl_id, p_is_abstract, p_is_dependent) ON CONFLICT DO NOTHING;
  WHEN TOO_MANY_ROWS THEN
    RAISE EXCEPTION 'record_decl not unique';
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION get_method_dependence(p_method_id integer,
                                                 p_callee_id integer) RETURNS void AS $$
DECLARE
  myrec cpp_doc_immutability_method_dependence%ROWTYPE;
BEGIN
  SELECT method_id, callee_id INTO STRICT myrec FROM cpp_doc_immutability_method_dependence WHERE method_id = p_method_id AND callee_id = p_callee_id;
EXCEPTION
  WHEN NO_DATA_FOUND THEN
    INSERT INTO cpp_doc_immutability_method_dependence (method_id, callee_id) VALUES (p_method_id, p_callee_id) ON CONFLICT DO NOTHING;
  WHEN TOO_MANY_ROWS THEN
    RAISE EXCEPTION 'immutability_method_dependence not unique';
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION get_clang_immutability_check_method(p_method_id integer, p_mutate_result integer, p_return_result integer) RETURNS void AS $$
BEGIN
  INSERT INTO cpp_doc_clang_immutability_check_method (method_id, mutate_result, return_result) VALUES (p_method_id, p_mutate_result, p_return_result)
  ON CONFLICT (method_id) DO UPDATE SET mutate_result = p_mutate_result, return_result = p_return_result;
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION get_clang_immutability_check_field(p_field_id integer, p_is_explicit boolean, p_is_transitive boolean) RETURNS void AS $$
BEGIN
  INSERT INTO cpp_doc_clang_immutability_check_field (field_id, is_explicit, is_transitive) VALUES (p_field_id, p_is_explicit, p_is_transitive)
  ON CONFLICT (field_id) DO UPDATE SET is_explicit = p_is_explicit, is_transitive = p_is_transitive;
END;
$$ LANGUAGE plpgsql;


CREATE OR REPLACE FUNCTION get_public_view(p_record_id integer, p_decl_id integer) RETURNS void AS $$
BEGIN
  INSERT INTO cpp_doc_public_view (record_id, decl_id) VALUES (p_record_id, p_decl_id) ON CONFLICT DO NOTHING;
END;
$$ LANGUAGE plpgsql;
