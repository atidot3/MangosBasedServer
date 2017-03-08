#ifndef ORIGIN_SINGLETON_H
#define ORIGIN_SINGLETON_H

/**
* @brief class Singleton
*/

#include "CreationPolicy.h"
#include "ThreadingModel.h"
#include "ObjectLifeTime.h"

namespace Origin
{
	template
		<
		typename T,
		class ThreadingModel = Origin::SingleThreaded<T>,
		class CreatePolicy = Origin::OperatorNew<T>,
		class LifeTimePolicy = Origin::ObjectLifeTime<T>
		>
		class Singleton
	{
	public:

		static T& Instance();

	protected:

		Singleton()
		{
		}

	private:

		// Prohibited actions...this does not prevent hijacking.
		Singleton(const Singleton&);
		Singleton& operator=(const Singleton&);

		// Singleton Helpers
		static void DestroySingleton();

		// data structure
		typedef typename ThreadingModel::Lock Guard;
		static T* si_instance;
		static bool si_destroyed;
	};

	template<typename T, class ThreadingModel, class CreatePolicy, class LifeTimePolicy>
	T* Singleton<T, ThreadingModel, CreatePolicy, LifeTimePolicy>::si_instance = nullptr;

	template<typename T, class ThreadingModel, class CreatePolicy, class LifeTimePolicy>
	bool Singleton<T, ThreadingModel, CreatePolicy, LifeTimePolicy>::si_destroyed = false;

	template<typename T, class ThreadingModel, class CreatePolicy, class LifeTimePolicy>
	T& Origin::Singleton<T, ThreadingModel, CreatePolicy, LifeTimePolicy>::Instance()
	{
		if (!si_instance)
		{
			// double-checked Locking pattern
			Guard();

			if (!si_instance)
			{
				if (si_destroyed)
				{
					si_destroyed = false;
					LifeTimePolicy::OnDeadReference();
				}

				si_instance = CreatePolicy::Create();
				LifeTimePolicy::ScheduleCall(&DestroySingleton);
			}
		}

		return *si_instance;
	}

	template<typename T, class ThreadingModel, class CreatePolicy, class LifeTimePolicy>
	void Origin::Singleton<T, ThreadingModel, CreatePolicy, LifeTimePolicy>::DestroySingleton()
	{
		CreatePolicy::Destroy(si_instance);
		si_instance = nullptr;
		si_destroyed = true;
	}
}

#define INSTANTIATE_SINGLETON_1(TYPE) \
    template class Origin::Singleton<TYPE, Origin::SingleThreaded<TYPE>, Origin::OperatorNew<TYPE>, Origin::ObjectLifeTime<TYPE> >;

#define INSTANTIATE_SINGLETON_2(TYPE, THREADINGMODEL) \
    template class Origin::Singleton<TYPE, THREADINGMODEL, Origin::OperatorNew<TYPE>, Origin::ObjectLifeTime<TYPE> >;

#define INSTANTIATE_SINGLETON_3(TYPE, THREADINGMODEL, CREATIONPOLICY ) \
    template class Origin::Singleton<TYPE, THREADINGMODEL, CREATIONPOLICY, Origin::ObjectLifeTime<TYPE> >;

#define INSTANTIATE_SINGLETON_4(TYPE, THREADINGMODEL, CREATIONPOLICY, OBJECTLIFETIME) \
    template class Origin::Singleton<TYPE, THREADINGMODEL, CREATIONPOLICY, OBJECTLIFETIME >;

#endif
