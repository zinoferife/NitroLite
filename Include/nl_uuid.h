#pragma once
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <nl_types.h>
#include <array>
#include <fmt/format.h>

namespace nl
{
	//nl uuid uses boost::uuid 
	class uuid : public boost::uuids::uuid
	{
	public:
		uuid(): boost::uuids::uuid(boost::uuids::nil_uuid()) {}
		void generate()
		{
			auto id = boost::uuids::random_generator_mt19937()();
			boost::uuids::uuid::operator=(id);
		}


		explicit uuid(boost::uuids::uuid const& u)
			: boost::uuids::uuid(u)
		{}

		operator boost::uuids::uuid() {
			return static_cast<boost::uuids::uuid&>(*this);
		}

		operator boost::uuids::uuid() const {
			return static_cast<boost::uuids::uuid const&>(*this);
		}

		nl::blob_t to_blob()
		{
			nl::blob_t blob(size());
			std::copy(begin(), end(), blob.begin());
			return std::move(blob);
		}

		void from_blob(const nl::blob_t& blob)
		{
			assert(!blob.empty());
			std::copy(blob.begin(), blob.end(), begin());
		}

		static const char* to_string(const uuid& id)
		{
			static constexpr const char* text = "0123456789ABCDEF";
			static std::array<char, 33> characters = {};
			std::uint8_t bottom_mask = 0x0F;
			std::uint8_t top_mask = 0xF0;
			size_t i = 0;
			for (auto iter = id.begin(); iter != id.end(); iter++, i+=2)
			{
				std::uint8_t low = (*iter & bottom_mask);
				std::uint8_t high = (*iter & top_mask);
				high >>= 4;
				characters[i] = text[low];
				characters[i + 1] = text[high];
			}
			return characters.data();
		}
	};

}

//allow fmt format uuid
namespace fmt
{
	template<>
	struct formatter<nl::uuid>
	{
		char presentation = 'q';
		constexpr auto parse(format_parse_context& context) -> decltype(context.begin())
		{
			auto it = context.begin(), end = context.end();
			if (it != end && (*it == 'q')) presentation = *it++;
			if (it != end && *it != '}') throw format_error("Invalid format");
			return it;
		}

		template<typename FormatContext>
		auto  format(const nl::uuid& uuid, FormatContext& ctx) -> decltype(ctx.out())
		{
			return format_to(ctx.out(), "{}", nl::uuid::to_string(uuid));
		}
	};
}
