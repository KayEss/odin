/**
    Copyright 2016-2018 Felspar Co Ltd. <http://odin.felspar.com/>

    Distributed under the Boost Software License, Version 1.0.
    See <http://www.boost.org/LICENSE_1_0.txt>
*/

#include <odin/facebook.hpp>
#include <odin/odin.hpp>
#include <odin/nonce.hpp>
#include <odin/user.hpp>
#include <odin/views.hpp>

#include <fost/exception/parse_error.hpp>
#include <fost/insert>
#include <fost/log>
#include <fost/mailbox>
#include <fostgres/sql.hpp>


namespace {

    const fostlib::module c_odin_facebook(odin::c_odin, "facebook.cpp");

    const class facebook : public fostlib::urlhandler::view {
    public:
        facebook()
        : view("odin.facebook.login") {
        }

        std::pair<boost::shared_ptr<fostlib::mime>, int> operator () (
            const fostlib::json &config,
            const fostlib::string &path,
            fostlib::http::server::request &req,
            const fostlib::host &host
        ) const {
            if ( req.method() != "POST" )
                throw fostlib::exceptions::not_implemented(__func__,
                    "Registration requires POST. This should be a 405");
            auto body_str = fostlib::coerce<fostlib::string>(
                fostlib::coerce<fostlib::utf8_string>(req.data()->data()));
            fostlib::json body = fostlib::json::parse(body_str);
            if ( !body.has_key("access_token") )
                throw fostlib::exceptions::not_implemented("odin.facebook.login",
                    "Must pass access_token field");
            const auto access_token = fostlib::coerce<fostlib::string>(body["access_token"]);
            auto facebook_app_token = odin::facebook::get_app_token();
            if ( !odin::facebook::does_user_authenticated(facebook_app_token, access_token) )
                throw fostlib::exceptions::not_implemented("odin.facebook.login",
                    "User not authenticated");

            auto user_detail = odin::facebook::get_user_detail(access_token);

            fostlib::pg::connection cnx{fostgres::connection(config, req)};
            const auto reference = odin::reference();
            const auto facebook_user_id = fostlib::coerce<f5::u8view>(user_detail["id"]);

            auto facebook_user = odin::facebook::credentials(cnx, facebook_user_id);

            auto identity_id = reference;
            if ( facebook_user.isnull() ) {
                odin::create_user(cnx, reference, identity_id);
            } else {
                identity_id = fostlib::coerce<fostlib::string>(facebook_user["identity"]["id"]);
            }
            odin::facebook::set_facebook_credential(cnx, reference, identity_id, facebook_user_id);

            cnx.commit();
            fostlib::mime::mime_headers headers;
            boost::shared_ptr<fostlib::mime> response(
                new fostlib::text_body(fostlib::json::unparse(user_detail, true),
                    headers, L"application/json"));
            return std::make_pair(response, 200);
        }
    } c_facebook;


}


const fostlib::urlhandler::view &odin::view::facebook = c_facebook;
