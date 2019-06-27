/*
    Copyright 2016 Felspar Co Ltd. http://odin.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <odin/odin.hpp>
#include <odin/views.hpp>
#include <odin/credentials.hpp>
#include <fost/test>

FSL_TEST_SUITE(credentials);

namespace {
    fostlib::json configuration() {
        fostlib::json config;
        fostlib::insert(config, "expires", "hours", 72);
        return config;
    }

    const fostlib::timestamp c_epoch(1970, 1, 1);
}

FSL_TEST_FUNCTION(check_can_renew_jwt_with_non_app_jwt) {

    fostlib::json payload;
    fostlib::insert(payload, "sub", "user01");
    fostlib::insert(payload, "fullname", "test01");
    const auto old_exp = fostlib::timestamp::now() + boost::posix_time::hours(24);
    fostlib::insert(payload, "exp", (old_exp - c_epoch).total_seconds());
    fostlib::jwt::mint jwt(fostlib::jwt::alg::HS256, payload);
    
    fostlib::http::server::request req("GET", "/");
    req.headers().set(
            "Authorization",
            ("Bearer " + jwt.token(odin::c_jwt_secret.value().data())).c_str());
    req.headers().set("__user", "user01");

    auto response = odin::view::jwt_renewal(configuration(), "/", req, fostlib::host());
    auto &rs = response.first;
    auto jwt_token = rs->body_as_string();
    auto load_jwt = fostlib::jwt::token::load(odin::c_jwt_secret.value(), jwt_token);

    FSL_CHECK(load_jwt.has_value());
    FSL_CHECK_EQ(load_jwt->payload["sub"], payload["sub"]);
    FSL_CHECK_EQ(load_jwt->payload["fullname"], payload["fullname"]);
    FSL_CHECK_NEQ(load_jwt->payload["exp"], payload["exp"]);
    FSL_CHECK_EQ(response.second, 200);
}

FSL_TEST_FUNCTION(check_can_renew_jwt_with_app_jwt) {
    fostlib::string app_id= "app01";
    fostlib::json payload;
    fostlib::insert(payload, "iss", "http://odin.felspar.com/app/app01");
    fostlib::insert(payload, "sub", "user01");
    fostlib::insert(payload, "app", app_id);
    const auto old_exp = fostlib::timestamp::now() + boost::posix_time::hours(24);
    fostlib::insert(payload, "exp", (old_exp - c_epoch).total_seconds());
    fostlib::jwt::mint jwt(fostlib::jwt::alg::HS256, payload);
    fostlib::http::server::request req("GET", "/");
    auto secret = odin::c_jwt_secret.value() + app_id;
    req.headers().set(
            "Authorization",
            ("Bearer " + jwt.token(secret.data())).c_str());
    req.headers().set("__user", "user01");
    req.headers().set("__app", app_id);

    auto response = odin::view::jwt_renewal(configuration(), "/", req, fostlib::host());
    
    auto &rs = response.first;
    auto jwt_token = rs->body_as_string();
    auto load_jwt = fostlib::jwt::token::load(secret, jwt_token);

    FSL_CHECK(load_jwt.has_value());
    FSL_CHECK_EQ(load_jwt->payload["iss"], payload["iss"]);
    FSL_CHECK_EQ(load_jwt->payload["sub"], payload["sub"]);
    FSL_CHECK_EQ(load_jwt->payload["app"], payload["app"]);
    FSL_CHECK_EQ(load_jwt->payload["exp"], load_jwt->payload["exp"]);
    FSL_CHECK_EQ(response.second, 200);
}
