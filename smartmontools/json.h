/*
 * json.h
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2017-21 Christian Franke
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef JSON_H_CVSID
#define JSON_H_CVSID "$Id$"

#include <stdint.h>
#include <stdio.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

/// Create and print JSON output.
class json
{
private:
  struct node_info
  {
    std::string key;
    int index = 0;

    node_info() = default;
    explicit node_info(const char * key_) : key(key_) { }
    explicit node_info(int index_) : index(index_) { }
  };

  typedef std::vector<node_info> node_path;

public:
  /// Return true if value is a safe JSON integer.
  /// Same as Number.isSafeInteger(value) in JavaScript.
  static bool is_safe_uint(unsigned long long value)
    { return (value < (1ULL << 53)); }

  /// Reference to a JSON element.
  class ref
  {
  public:
    ~ref();

    /// Return reference to object element.
    ref operator[](const char * key) const
      { return ref(*this, key); }

    /// Return reference to array element.
    ref operator[](int index) const
      { return ref(*this, index); }

    // Assignment operators create or change element.
    void operator=(bool value);

    void operator=(int value);
    void operator=(unsigned value);
    void operator=(long value);
    void operator=(unsigned long value);
    void operator=(long long value);
    void operator=(unsigned long long value);

    void operator=(const char * value);
    void operator=(const std::string & value);

    /// Return reference to element with KEY_SUFFIX appended to last key.
    ref with_suffix(const char * key_suffix) const
      { return ref(*this, "", key_suffix); }

    void set_uint128(uint64_t value_hi, uint64_t value_lo);

    // Output only if safe integer.
    bool set_if_safe_uint64(uint64_t value);
    bool set_if_safe_uint128(uint64_t value_hi, uint64_t value_lo);
    bool set_if_safe_le128(const void * pvalue);

    // If unsafe integer, output also as string with key "NUMBER_s".
    void set_unsafe_uint64(uint64_t value);
    void set_unsafe_uint128(uint64_t value_hi, uint64_t value_lo);
    void set_unsafe_le128(const void * pvalue);

  private:
    friend class json;
    ref(json & js, const char * key);
    ref(const ref & base, const char * key);
    ref(const ref & base, int index);
    ref(const ref & base, const char * /*dummy*/, const char * key_suffix);

    json & m_js;
    node_path m_path;
  };

  /// Return reference to element of top level object.
  ref operator[](const char * key)
    { return ref(*this, key); }

  /// Enable/disable JSON output.
  void enable(bool yes = true)
    { m_enabled = yes; }

  /// Return true if enabled.
  bool is_enabled() const
    { return m_enabled; }

  /// Enable/disable extra string output for safe integers also.
  void set_verbose(bool yes = true)
    { m_verbose = yes; }

  /// Return true if any 128-bit value has been output.
  bool has_uint128_output() const
    { return m_uint128_output; }

  /// Options for print().
  struct print_options {
    bool pretty = false; //< Pretty-print output.
    bool sorted = false; //< Sort object keys.
    char format = 0; //< 'y': YAML, 'g': flat(grep, gron), other: JSON
  };

  /// Print JSON tree to a file.
  void print(FILE * f, const print_options & options) const;

private:
  enum node_type {
    nt_unset, nt_object, nt_array,
    nt_bool, nt_int, nt_uint, nt_uint128, nt_string
  };

  struct node
  {
    node();
    node(const node &) = delete;
    explicit node(const std::string & key_);
    ~node();
    void operator=(const node &) = delete;

    node_type type = nt_unset;

    uint64_t intval = 0, intval_hi = 0;
    std::string strval;

    std::string key;
    std::vector< std::unique_ptr<node> > childs;
    typedef std::map<std::string, unsigned> keymap;
    keymap key2index;

    class const_iterator
    {
    public:
      const_iterator(const node * node_p, bool sorted);
      bool at_end() const;
      unsigned array_index() const;
      void operator++();
      const node * operator*() const;

    private:
      const node * m_node_p;
      bool m_use_map;
      unsigned m_child_idx = 0;
      keymap::const_iterator m_key_iter;
    };
  };

  bool m_enabled = false;
  bool m_verbose = false;
  bool m_uint128_output = false;

  node m_root_node;

  node * find_or_create_node(const node_path & path, node_type type);

  void set_bool(const node_path & path, bool value);
  void set_int64(const node_path & path, int64_t value);
  void set_uint64(const node_path & path, uint64_t value);
  void set_uint128(const node_path & path, uint64_t value_hi, uint64_t value_lo);
  void set_cstring(const node_path & path, const char * value);
  void set_string(const node_path & path, const std::string & value);

  static void print_json(FILE * f, bool pretty, bool sorted, const node * p, int level);
  static void print_yaml(FILE * f, bool pretty, bool sorted, const node * p, int level_o,
                         int level_a, bool cont);
  static void print_flat(FILE * f, const char * assign, bool sorted, const node * p,
                         std::string & path);
};

#endif // JSON_H_CVSID
