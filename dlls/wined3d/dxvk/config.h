/*
                  Copyright (c) 2017-2019 Philip Rebohle

                          zlib/libpng license

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

– The origin of this software must not be misrepresented; you must not
  claim that you wrote the original software. If you use this software
  in a product, an acknowledgment in the product documentation would be
  appreciated but is not required.

– Altered source versions must be plainly marked as such, and must not
  be misrepresented as being the original software.

– This notice may not be removed or altered from any source distribution.
*/

#pragma once

#include <string>
#include <unordered_map>

namespace dxvk {

  std::string getExePath();
  int32_t parsePciId(const std::string& str);

  /**
   * \brief Tri-state
   * 
   * Used to conditionally override
   * booleans if desired.
   */
  enum class Tristate : int32_t {
    Auto  = -1,
    False =  0,
    True  =  1,
  };

  /**
   * \brief Config option set
   * 
   * Stores configuration options
   * as a set of key-value pairs.
   */
  class Config {
    using OptionMap = std::unordered_map<std::string, std::string>;
  public:

    Config();
    Config(OptionMap&& options);
    ~Config();

    /**
     * \brief Merges two configuration sets
     * 
     * Options specified in this config object will
     * not be overridden if they are specified in
     * the second config object.
     * \param [in] other Config set to merge.
     */
    void merge(const Config& other);

    /**
     * \brief Sets an option
     * 
     * \param [in] key Option name
     * \param [in] value Option value
     */
    void setOption(
      const std::string& key,
      const std::string& value);

    /**
     * \brief Parses an option value
     *
     * Retrieves the option value as a string, and then
     * tries to convert that string to the given type.
     * If parsing the string fails because it is either
     * invalid or if the option is not defined, this
     * method will return a fallback value.
     * 
     * Currently, this supports the types \c bool,
     * \c int32_t, and \c std::string.
     * \tparam T Return value type
     * \param [in] option Option name
     * \param [in] fallback Fallback value
     * \returns Parsed option value
     * \returns The parsed option value
     */
    template<typename T>
    T getOption(const char* option, T fallback = T()) const {
      const std::string& value = getOptionValue(option);

      T result = fallback;
      parseOptionValue(value, result);
      return result;
    }

    /**
     * \brief Logs option values
     * 
     * Prints the effective configuration
     * to the log for debugging purposes. 
     */
    void logOptions() const;

    /**
     * \brief Retrieves default options for an app
     * 
     * \param [in] appName Name of the application
     * \returns Default options for the application
     */
    static Config getAppConfig(const std::string& appName);

    /**
     * \brief Retrieves user configuration
     * 
     * Reads options from the configuration file,
     * if it can be found, or an empty option set.
     * \returns User-defined configuration options
     */
    static Config getUserConfig();

  private:

    OptionMap m_options;

    std::string getOptionValue(
      const char*         option) const;

    static bool parseOptionValue(
      const std::string&  value,
            std::string&  result);

    static bool parseOptionValue(
      const std::string&  value,
            bool&         result);

    static bool parseOptionValue(
      const std::string&  value,
            int32_t&      result);
    
    static bool parseOptionValue(
      const std::string&  value,
            Tristate&     result);

  };


  /**
   * \brief Applies tristate option
   * 
   * Overrides the given value if \c state is
   * \c True or \c False, and leaves it intact
   * otherwise.
   * \param [out] option The value to override
   * \param [in] state Tristate to apply
   */
  inline void applyTristate(bool& option, Tristate state) {
    option &= state != Tristate::False;
    option |= state == Tristate::True;
  }

}
