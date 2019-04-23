/**
    Copyright 2018 Felspar Co Ltd. <http://odin.felspar.com/>

    Distributed under the Boost Software License, Version 1.0.
    See <http://www.boost.org/LICENSE_1_0.txt>
*/

#include <odin/facebook.hpp>
#include <odin/fg/native.hpp>
#include <odin/odin.hpp>

#include <fost/http>
#include <fost/log>
#include <fostgres/sql.hpp>

#include <fost/insert>


f5::u8string odin::facebook::get_app_token(
        const f5::u8string &app_id, const f5::u8string &app_secret) {
    fostlib::url base_url(odin::c_facebook_endpoint.value());
    fostlib::url::filepath_string api{"/oauth/access_token"};
    fostlib::url::query_string qs{};
    fostlib::url fb_url(base_url, api);
    qs.append("client_id", app_id);
    qs.append("client_secret", app_secret);
    qs.append("grant_type", "client_credentials");
    fb_url.query(qs);
    fostlib::http::user_agent ua(fb_url);
    auto response = ua.get(fb_url);
    auto response_data = fostlib::coerce<fostlib::string>(
            fostlib::coerce<fostlib::utf8_string>(response->body()->data()));
    fostlib::json body = fostlib::json::parse(response_data);
    return fostlib::coerce<f5::u8string>(body["access_token"]);
}


bool odin::facebook::is_user_authenticated(
        f5::u8view app_token, f5::u8view user_token) {
    fostlib::url base_url(odin::c_facebook_endpoint.value());
    fostlib::url::filepath_string api{"/debug_token"};
    fostlib::url fb_url(base_url, api);
    fostlib::url::query_string qs{};
    qs.append("input_token", user_token);
    qs.append("access_token", app_token);
    fb_url.query(qs);
    fostlib::http::user_agent ua(fb_url);
    auto response = ua.get(fb_url);
    if (response->status() != 200) {
        return false;
    }
    auto response_data = fostlib::coerce<fostlib::string>(
            fostlib::coerce<fostlib::utf8_string>(response->body()->data()));
    fostlib::json body = fostlib::json::parse(response_data);
    return fostlib::coerce<bool>(body["data"]["is_valid"]);
}


fostlib::json odin::facebook::get_user_detail(f5::u8view user_token) {
    fostlib::url base_url(odin::c_facebook_endpoint.value());
    fostlib::url::filepath_string api{"/me"};
    auto fb_apps = odin::c_facebook_apps.value();
    for (const auto fb_app : fb_apps) {
        const auto app_token = get_app_token(
                fostlib::coerce<f5::u8string>(fb_app["app_id"]),
                fostlib::coerce<f5::u8string>(fb_app["app_secret"]));
        if (is_user_authenticated(app_token, user_token)) {
            fostlib::url fb_url(base_url, api);
            fostlib::url::query_string qs{};
            qs.append("access_token", user_token);
            qs.append("fields", "id,name,email");
            fb_url.query(qs);
            fostlib::http::user_agent ua(fb_url);
            auto response = ua.get(fb_url);
            auto response_data = fostlib::coerce<fostlib::string>(
                    fostlib::coerce<fostlib::utf8_string>(
                            response->body()->data()));
            return fostlib::json::parse(response_data);
        }
    }
    return fostlib::json{};
}


fostlib::json odin::facebook::credentials(
        fostlib::pg::connection &cnx, const f5::u8view &user_id) {
    static const fostlib::string sql(
            "SELECT "
            "odin.identity.tableoid AS identity__tableoid, "
            "odin.facebook_credentials.tableoid AS "
            "facebook_credentials__tableoid, "
            "odin.identity.*, odin.facebook_credentials.* "
            "FROM odin.facebook_credentials "
            "JOIN odin.identity ON "
            "odin.identity.id=odin.facebook_credentials.identity_id "
            "WHERE odin.facebook_credentials.facebook_user_id = $1");
    auto data = fostgres::sql(cnx, sql, std::vector<fostlib::string>{user_id});
    auto &rs = data.second;
    auto row = rs.begin();
    if (row == rs.end()) {
        fostlib::log::warning(c_odin)("", "Facebook user not found")(
                "facebook_user_id", user_id);
        return fostlib::json();
    }
    auto record = *row;
    if (++row != rs.end()) {
        fostlib::log::error(c_odin)("", "More than one facebook user returned")(
                "facebook_user_id", user_id);
        return fostlib::json();
    }

    fostlib::json user;
    for (std::size_t index{0}; index < record.size(); ++index) {
        const auto parts = fostlib::split(data.first[index], "__");
        if (parts.size() && parts[parts.size() - 1] == "tableoid") continue;
        fostlib::jcursor pos;
        for (const auto &p : parts) pos /= p;
        fostlib::insert(user, pos, record[index]);
    }
    return user;
}


void odin::facebook::set_facebook_credentials(
        fostlib::pg::connection &cnx,
        f5::u8view reference,
        f5::u8view identity_id,
        f5::u8view facebook_user_id) {
    fg::json user_values;
    fostlib::insert(user_values, "reference", reference);
    fostlib::insert(user_values, "identity_id", identity_id);
    fostlib::insert(user_values, "facebook_user_id", facebook_user_id);
    cnx.insert("odin.facebook_credentials_ledger", user_values);
}
